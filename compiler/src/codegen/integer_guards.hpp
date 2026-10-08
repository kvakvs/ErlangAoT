#pragma once
#include <llvm/IR/IRBuilder.h>
#include <map>

namespace clause::codegen {
// Associate exact, side-effect-free entry-block ABI tag checks with original argument slots.
using IntegerGuards = std::map<llvm::ICmpInst *, std::size_t>;
IntegerGuards integer_guards(llvm::Function &function, std::size_t arity);
} // namespace clause::codegen
