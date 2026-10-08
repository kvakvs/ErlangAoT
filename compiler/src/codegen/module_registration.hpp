#pragma once
#include "../semantic/declarations.hpp"
#include <llvm/IR/Module.h>

namespace clause::codegen {
// Emit immutable descriptors and explicit startup, retaining their runtime dependency through object emission.
void emit_registration(llvm::Module &output, const semantic::Module &module, llvm::IntegerType *word);
} // namespace clause::codegen
