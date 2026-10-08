#pragma once
#include "specialization.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

namespace clause::codegen {
struct SpecializedVariant {
    // Borrow a module-owned clone and the entry proofs required to invoke it safely.
    llvm::Function *function;
    TypeProfile profile;
};

struct SpecializedDispatch {
    // Retain an unpublished dispatcher and its explicit generic fallback for transactional installation.
    llvm::Function *function;
    llvm::CallInst *fallback;
};

// Clone with the same ABI and remove only exact checks established by the profile; null means no benefit.
llvm::Function *clone_variant(llvm::Function &generic, const TypeProfile &profile);
// Guard each constrained argument before invoking a clone; every miss reaches the original generic body.
SpecializedDispatch create_dispatch(llvm::Function &generic, std::span<const SpecializedVariant> variants);
// Measure actual added IR before replacing public entries; reject excess growth without failing compilation.
void lower_specializations(llvm::Module &module, SpecializationPlan &plan);
} // namespace clause::codegen
