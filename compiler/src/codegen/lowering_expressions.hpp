#pragma once
#include "../semantic/types/inference.hpp"
#include <llvm/IR/IRBuilder.h>

namespace erlang_aot::codegen {
// Emit source-ordered candidates and a single exhaustion exit without trusting specifications.
void lower_function(llvm::IRBuilder<> &builder, llvm::Function &entry, const semantic::Module &module,
                    const semantic::Function &function, llvm::IntegerType *word,
                    const semantic::types::Inference &inferred);
} // namespace erlang_aot::codegen
