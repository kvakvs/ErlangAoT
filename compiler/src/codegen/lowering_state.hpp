#pragma once
#include "lowering_expressions.hpp"
#include <map>

namespace erlang_aot::codegen {
struct ExpressionLowering {
    // Borrow the current generic entry, immutable source/analysis and target-word builder.
    llvm::IRBuilder<> &builder;
    llvm::Function &entry;
    const semantic::Module &module;
    const semantic::Function &function;
    const semantic::types::Inference &inferred;
    llvm::IntegerType *word;
    // Keep source-node results for iterative, left-to-right argument evaluation.
    std::map<const ast::Expression *, llvm::Value *> values;
};

// Emit one resolved call after its arguments have been evaluated in source order.
llvm::Value *lower_call(ExpressionLowering &state, const ast::Expression &expression, const ast::CallExpression &call);
} // namespace erlang_aot::codegen
