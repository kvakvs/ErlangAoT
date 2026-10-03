#include "lowering_state.hpp"
#include "runtime_symbols.hpp"
#include <erlang_aot/abi/term.hpp>
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/SSAUpdater.h>

namespace erlang_aot::codegen {
namespace {
using Op = abi::v1::ImmediateOperation;

// Twice the target width holds every exact add/subtract/product of two small payloads.
std::optional<llvm::Instruction::BinaryOps> arithmetic(Op operation) {
    switch (operation) {
    case Op::add:
        return llvm::Instruction::Add;
    case Op::subtract:
        return llvm::Instruction::Sub;
    case Op::multiply:
        return llvm::Instruction::Mul;
    default:
        return {};
    }
}

// Reject noninteger tags before decoding, independently of inferred facts or source specifications.
llvm::Value *compare(ExpressionLowering &state, llvm::CmpInst::Predicate operation, llvm::Value *left,
                     llvm::Value *right) {
    return state.builder.Insert(llvm::CmpInst::Create(llvm::Instruction::ICmp, operation, left, right));
}

// Check both immediate tags before any payload decode is reachable.
llvm::Value *small(ExpressionLowering &state, llvm::Value *value) {
    auto &builder = state.builder;
    return compare(state, llvm::CmpInst::ICMP_EQ, builder.CreateAnd(value, llvm::ConstantInt::get(state.word, 15)),
                   llvm::ConstantInt::get(state.word, 15));
}

struct Arithmetic {
    // Carry the checked decoded result and its combined word/payload overflow proof.
    llvm::Value *value;
    llvm::Value *valid;
};

// Compute without wrap in a double-width integer, then prove payload bounds before truncation and encoding.
Arithmetic calculate(ExpressionLowering &state, llvm::Instruction::BinaryOps operation, llvm::Value *left,
                     llvm::Value *right) {
    auto &builder = state.builder;
    auto *wide = builder.getIntNTy(state.word->getBitWidth() * 2);
    auto *shift = llvm::ConstantInt::get(state.word, 4);
    auto *lhs = builder.CreateSExt(builder.CreateAShr(left, shift), wide);
    auto *rhs = builder.CreateSExt(builder.CreateAShr(right, shift), wide);
    auto *number = builder.CreateBinOp(operation, lhs, rhs, "integer.checked");
    const auto minimum =
        state.word->getBitWidth() == 32 ? abi::v1::IntegerEncoding<32>::minimum : abi::v1::IntegerEncoding<64>::minimum;
    const auto maximum = -minimum - 1;
    auto *fits =
        builder.CreateAnd(compare(state, llvm::CmpInst::ICMP_SGE, number, llvm::ConstantInt::getSigned(wide, minimum)),
                          compare(state, llvm::CmpInst::ICMP_SLE, number, llvm::ConstantInt::getSigned(wide, maximum)));
    return {number, fits};
}

// Share LLVM's SSA formation utility with lazy joins, preserving locations on its inserted PHI.
llvm::Value *joined(ExpressionLowering &state, llvm::BasicBlock *fast, llvm::Value *encoded, llvm::BasicBlock *slow,
                    llvm::Value *promoted) {
    auto &builder = state.builder;
    auto *boundary = builder.CreateUnreachable();
    llvm::SmallVector<llvm::PHINode *, 2> phis;
    llvm::SSAUpdater updater(&phis);
    updater.Initialize(state.word, "integer.value");
    updater.AddAvailableValue(fast, encoded);
    updater.AddAvailableValue(slow, promoted);
    auto *value = updater.GetValueInMiddleOfBlock(builder.GetInsertBlock());
    for (auto *phi : phis) {
        phi->setDebugLoc(builder.getCurrentDebugLocation());
    }
    boundary->eraseFromParent();
    return value;
}
} // namespace

llvm::BasicBlock *bad_arithmetic_exit(ExpressionLowering &state) {
    if (state.rejection) {
        return state.rejection;
    }
    if (!state.bad_arithmetic) {
        auto *saved = state.builder.GetInsertBlock();
        state.bad_arithmetic = llvm::BasicBlock::Create(state.entry.getContext(), "body.badarith", &state.entry);
        state.builder.SetInsertPoint(state.bad_arithmetic);
        raise_reason(state, abi::v1::ErrorReason::badarith);
        state.builder.SetInsertPoint(saved);
    }
    return state.bad_arithmetic;
}

llvm::Value *lower_integer(ExpressionLowering &state, std::string_view decimal) {
    auto &output = *state.entry.getParent();
    auto &builder = state.builder;
    auto *bytes = llvm::ConstantDataArray::getString(output.getContext(), decimal, false);
    auto *text = new llvm::GlobalVariable(output, bytes->getType(), true, llvm::GlobalValue::PrivateLinkage, bytes,
                                          "integer.literal");
    auto *slot = root_slot(state);
    auto service = output.getOrInsertFunction(
        services::symbol<services::Integer>(output.getTargetTriple()),
        llvm::FunctionType::get(builder.getInt8Ty(),
                                {builder.getPtrTy(), builder.getPtrTy(), state.word, builder.getPtrTy()}, false));
    auto *outcome = builder.CreateCall(
        service, {state.entry.getArg(0), text, llvm::ConstantInt::get(state.word, decimal.size()), slot},
        "integer.outcome");
    return checked_value(state, {outcome, slot}, bad_arithmetic_exit(state));
}

llvm::Value *lower_operation(ExpressionLowering &state, Op operation, llvm::Value *left, llvm::Value *right) {
    if (auto *map = lower_map_query(state, operation, left, right)) {
        return map;
    }
    const auto checked = arithmetic(operation);
    if (!checked) {
        return lower_immediate(state, operation, left, right);
    }
    auto &builder = state.builder;
    auto &context = state.entry.getContext();
    auto *attempt = llvm::BasicBlock::Create(context, "integer.small", &state.entry);
    auto *fast = llvm::BasicBlock::Create(context, "integer.small.success", &state.entry);
    auto *fallback = llvm::BasicBlock::Create(context, "integer.fallback", &state.entry);
    auto *join = llvm::BasicBlock::Create(context, "integer.join", &state.entry);
    builder.CreateCondBr(builder.CreateAnd(small(state, left), small(state, right)), attempt, fallback);
    builder.SetInsertPoint(attempt);
    const auto result = calculate(state, *checked, left, right);
    builder.CreateCondBr(result.valid, fast, fallback);
    builder.SetInsertPoint(fast);
    auto *payload = builder.CreateTrunc(result.value, state.word);
    auto *encoded = builder.CreateOr(builder.CreateShl(payload, llvm::ConstantInt::get(state.word, 4)),
                                     llvm::ConstantInt::get(state.word, 15));
    builder.CreateBr(join);
    builder.SetInsertPoint(fallback);
    auto *promoted = lower_immediate(state, operation, left, right);
    auto *slow = builder.GetInsertBlock();
    builder.CreateBr(join);
    builder.SetInsertPoint(join);
    return joined(state, fast, encoded, slow, promoted);
}
} // namespace erlang_aot::codegen
