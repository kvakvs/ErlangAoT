#include "lowering_state.hpp"
#include <llvm/IR/Module.h>
#include <stdexcept>

namespace erlang_aot::codegen {
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
    return result;
}
} // namespace erlang_aot::codegen
