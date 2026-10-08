#pragma once
#include "../semantic/declarations.hpp"
#include <llvm/IR/Module.h>

namespace clause::codegen {
// Collect executable literal spellings and assign module-local slots, never runtime atom IDs.
// The module and every function name are included for stack trace frames.
llvm::Constant *emit_atom_table(llvm::Module &output, const semantic::Module &module, llvm::IntegerType *word,
                                std::size_t &count);
// The slot index of an atom spelling collected by emit_atom_table.
llvm::Constant *atom_slot(llvm::Module &output, const std::string &spelling);
} // namespace clause::codegen
