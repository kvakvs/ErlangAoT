#pragma once
#include <llvm/IR/IRBuilder.h>
#include <map>

namespace clause::codegen {
// Associate exact small-integer tag checks of argument words, anywhere in the body, with their argument slots.
using IntegerGuards = std::map<llvm::ICmpInst *, std::size_t>;
IntegerGuards integer_guards(llvm::Function &function, std::size_t arity);
} // namespace clause::codegen
