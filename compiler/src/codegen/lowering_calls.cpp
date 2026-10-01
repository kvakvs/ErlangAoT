#include "lowering_state.hpp"
#include <llvm/IR/Module.h>
#include <llvm/TargetParser/Triple.h>
#include <stdexcept>

namespace erlang_aot::codegen {
namespace {
// Reuse one terminal exit; callers must inspect the channel before interpreting its invalid word.
llvm::BasicBlock *failure_exit(ExpressionLowering &state) {
    if (!state.failure) {
        state.failure = llvm::BasicBlock::Create(state.entry.getContext(), "call.failure", &state.entry);
        llvm::IRBuilder<> exit_builder(state.failure);
        exit_builder.CreateRet(llvm::ConstantInt::get(state.word, 0));
    }
    return state.failure;
}

// Check the context channel before a result can feed another argument or body operation.
} // namespace

void propagate_failure(ExpressionLowering &state) {
    auto &builder = state.builder;
    auto &output = *state.entry.getParent();
    const auto &triple = output.getTargetTriple();
    const auto symbol =
        triple.isWindowsMSVCEnvironment()
            ? (triple.isArch64Bit() ? "?erlang_aot_call_failed_v2@@YAEPEAX@Z" : "?erlang_aot_call_failed_v2@@YAEPAX@Z")
            : "_Z25erlang_aot_call_failed_v2Pv";
    auto service =
        output.getOrInsertFunction(symbol, llvm::FunctionType::get(builder.getInt8Ty(), {builder.getPtrTy()}, false));
    auto *failed = builder.CreateCall(service, {state.entry.getArg(0)}, "call.failed");
    auto *failure = failure_exit(state);
    auto *success = llvm::BasicBlock::Create(output.getContext(), "call.success", &state.entry);
    auto *check = builder.Insert(
        llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::CmpInst::ICMP_NE, failed, builder.getInt8(0)));
    builder.CreateCondBr(check, failure, success);
    builder.SetInsertPoint(success);
}

namespace {
// Allocate a word-aligned borrowed argument array; zero arity passes an unused null pointer.
llvm::Value *arguments(ExpressionLowering &state, const ast::CallExpression &call) {
    auto &builder = state.builder;
    if (call.arguments.empty()) {
        return llvm::ConstantPointerNull::get(builder.getPtrTy());
    }
    auto *count = llvm::ConstantInt::get(state.word, call.arguments.size());
    auto *array = builder.CreateAlloca(state.word, count, "call.arguments");
    const llvm::Align alignment(state.word->getBitWidth() / 8);
    array->setAlignment(alignment);
    for (std::size_t i = 0; i < call.arguments.size(); ++i) {
        auto *value = state.values.at(&state.module.syntax->expression(call.arguments[i]));
        auto *slot = builder.CreateGEP(state.word, array, llvm::ConstantInt::get(state.word, i));
        builder.CreateAlignedStore(value, slot, alignment);
    }
    return array;
}

// Import only batch-resolved exported entries, using exactly the current generic ABI signature.
llvm::Function *callee_declaration(ExpressionLowering &state, const semantic::FunctionRef callee) {
    auto &output = *state.entry.getParent();
    if (callee.module != &state.module && !callee.function->exported) {
        throw std::invalid_argument("lowering: remote callee is not exported");
    }
    if (auto *existing = output.getFunction(callee.function->symbol)) {
        return existing;
    }
    if (callee.module == &state.module) {
        throw std::invalid_argument("lowering: missing local declaration");
    }
    auto *entry = llvm::Function::Create(state.entry.getFunctionType(), llvm::GlobalValue::ExternalLinkage,
                                         callee.function->symbol, output);
    entry->setCallingConv(llvm::CallingConv::C);
    return entry;
}
} // namespace

llvm::Value *lower_call(ExpressionLowering &state, const ast::Expression &expression, const ast::CallExpression &call) {
    const auto callee = state.inferred.callees.at(&expression);
    // Consume the resolved identity and summary, never repeat Erlang name resolution here.
    const auto &summary = state.inferred.functions.at(callee.function);
    if (summary.inputs.size() != call.arguments.size()) {
        throw std::invalid_argument("lowering: inconsistent resolved call arity");
    }
    auto *target = callee_declaration(state, callee);
    auto *result = state.builder.CreateCall(target, {state.entry.getArg(0), arguments(state, call)}, "call.result");
    result->setCallingConv(llvm::CallingConv::C);
    propagate_failure(state);
    return result;
}
} // namespace erlang_aot::codegen
