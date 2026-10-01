#pragma once
#include "../semantic/declarations.hpp"
#include <llvm/IR/Module.h>

namespace erlang_aot::codegen {
// Collect executable literal spellings and assign module-local slots, never runtime atom IDs.
llvm::Constant *emit_atom_table(llvm::Module &output, const semantic::Module &module, llvm::IntegerType *word,
                                std::size_t &count);
} // namespace erlang_aot::codegen
