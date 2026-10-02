#include "lowering_state.hpp"
#include <llvm/IR/Module.h>
#include <llvm/TargetParser/Triple.h>

namespace erlang_aot::codegen {
namespace {
// Semantic rejection targets the entire enclosing guard; ordinary bodies use one shared badarg exit.
llvm::BasicBlock *bad_argument(ExpressionLowering &state) {
    if (state.rejection) {
        return state.rejection;
    }
    if (!state.bad_argument) {
        auto *saved = state.builder.GetInsertBlock();
        state.bad_argument = llvm::BasicBlock::Create(state.entry.getContext(), "body.badarg", &state.entry);
        state.builder.SetInsertPoint(state.bad_argument);
        raise_reason(state, abi::v1::ErrorReason::badarg);
        state.builder.SetInsertPoint(saved);
    }
    return state.bad_argument;
}

// Derive native C++ linker spelling from the emitted platform and word width.
std::string_view symbol(const llvm::Triple &triple) {
    if (triple.isWindowsMSVCEnvironment()) {
        return triple.isArch64Bit() ? "?erlang_aot_immediate_v1@@YAEPEAXE_K1PEA_K@Z"
                                    : "?erlang_aot_immediate_v1@@YAEPAXEIIPAI@Z";
    }
    return triple.isArch64Bit() ? "_Z23erlang_aot_immediate_v1PvhmmPm" : "_Z23erlang_aot_immediate_v1PvhjjPj";
}
} // namespace

llvm::Value *lower_immediate(ExpressionLowering &state, abi::v1::ImmediateOperation operation, llvm::Value *left,
                             llvm::Value *right) {
    auto &builder = state.builder;
    auto &output = *state.entry.getParent();
    const llvm::Align alignment(state.word->getBitWidth() / 8);
    auto *slot = builder.CreateAlloca(state.word, nullptr, "service.output");
    slot->setAlignment(alignment);
    auto service = output.getOrInsertFunction(
        symbol(output.getTargetTriple()),
        llvm::FunctionType::get(builder.getInt8Ty(),
                                {builder.getPtrTy(), builder.getInt8Ty(), state.word, state.word, builder.getPtrTy()},
                                false));
    auto *outcome = builder.CreateCall(service,
                                       {state.entry.getArg(0), builder.getInt8(static_cast<std::uint8_t>(operation)),
                                        left, right ? right : llvm::ConstantInt::get(state.word, 0), slot},
                                       "service.outcome");
    propagate_failure(state);
    auto *success = llvm::BasicBlock::Create(state.entry.getContext(), "service.success", &state.entry);
    auto *test = builder.Insert(
        llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::CmpInst::ICMP_EQ, outcome, builder.getInt8(0)));
    builder.CreateCondBr(test, success, bad_argument(state));
    builder.SetInsertPoint(success);
    return builder.CreateAlignedLoad(state.word, slot, alignment, "service.value");
}
} // namespace erlang_aot::codegen
