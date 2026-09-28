#pragma once
#include "../semantic/types/inference.hpp"
#include <llvm/IR/IRBuilder.h>

namespace erlang_aot::codegen {
// Return a generic term without narrowing its type from a source specification.
llvm::Value *lower_expression(llvm::IRBuilder<> &builder, llvm::Function &entry, const semantic::Module &module,
                              const semantic::Function &function, const ast::ExprId &expression,
                              llvm::IntegerType *word, const semantic::types::Inference &inferred);
} // namespace erlang_aot::codegen
