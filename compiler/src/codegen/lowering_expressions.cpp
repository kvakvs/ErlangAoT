#include "lowering_expressions.hpp"
#include "../semantic/capabilities.hpp"
#include <algorithm>
#include <erlang_aot/abi/term.hpp>
#include <stdexcept>

namespace erlang_aot::codegen {
namespace {
// Checked canonical decimal parsing rejects arbitrary-size values before LLVM sees them.
llvm::ConstantInt *literal(const ast::Module &syntax, const ast::ExprId &expression, llvm::IntegerType *word) {
    const auto value = semantic::integer_literal(syntax, expression, word->getBitWidth());
    if (!value) {
        throw std::invalid_argument("lowering: expected a representable integer literal");
    }
    const auto encoded = word->getBitWidth() == 32 ? *abi::v1::IntegerEncoding<32>::encode(*value)
                                                   : *abi::v1::IntegerEncoding<64>::encode(*value);
    return llvm::ConstantInt::get(word, encoded);
}

// Use the binding's original argument index, with target-word alignment and no inbounds promise.
llvm::Value *parameter(llvm::IRBuilder<> &builder, llvm::Function &entry, llvm::IntegerType *word,
                       const semantic::Binding &binding) {
    auto *slot =
        builder.CreateGEP(word, entry.getArg(1), llvm::ConstantInt::get(word, binding.argument), "argument.slot");
    return builder.CreateAlignedLoad(word, slot, llvm::Align(word->getBitWidth() / 8), "argument");
}
} // namespace

llvm::Value *lower_expression(llvm::IRBuilder<> &builder, llvm::Function &entry, const semantic::Module &module,
                              const semantic::Function &function, ast::ExprId expression, llvm::IntegerType *word) {
    expression = semantic::ungroup(*module.syntax, expression);
    const auto binding = std::ranges::find(function.bindings, expression, &semantic::Binding::expression);
    if (binding != function.bindings.end()) {
        return parameter(builder, entry, word, *binding);
    }
    return literal(*module.syntax, expression, word);
}
} // namespace erlang_aot::codegen
