#pragma once
#include "../semantic/declarations.hpp"
#include "compilation.hpp"
#include <llvm/IR/DerivedTypes.h>
#include <memory>
#include <span>

namespace clause::codegen {
// Append the startup module: a native `main` passing every module descriptor and the entry to the runtime.
void emit_startup(Compilation &compilation, std::span<const std::unique_ptr<semantic::Module>> modules,
                  llvm::IntegerType *word);
} // namespace clause::codegen
