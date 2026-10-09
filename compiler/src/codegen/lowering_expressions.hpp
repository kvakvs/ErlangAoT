#pragma once
#include "../semantic/types/inference.hpp"
#include "proofs.hpp"
#include <llvm/IR/IRBuilder.h>

namespace clause::codegen {
// Emit source-ordered candidates and a single exhaustion exit without trusting specifications; non-null `proofs`
// let inferred facts and shape tests replace checks (step 59).
void lower_function(llvm::IRBuilder<> &builder, llvm::Function &entry, const semantic::Module &module,
                    const semantic::Function &function, llvm::IntegerType *word,
                    const semantic::types::Inference &inferred, const Proofs *proofs = nullptr);
// Emit an anonymous fun's code: its clauses over its arguments, with its captured values after them.
void lower_lambda(llvm::IRBuilder<> &builder, llvm::Function &entry, const semantic::Module &module,
                  const semantic::FunEntry &lambda, llvm::IntegerType *word,
                  const semantic::types::Inference &inferred);
} // namespace clause::codegen
