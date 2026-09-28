#include "integer_guards.hpp"
#include "specialization_lowering.hpp"
#include <llvm/Analysis/InstructionSimplify.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/Local.h>

namespace erlang_aot::codegen {
namespace {
// Track deletion separately from RAUW while LLVM simplifies dependent expressions.
std::vector<llvm::WeakVH> proven_checks(llvm::Function &clone, const TypeProfile &profile) {
    std::vector<llvm::WeakVH> result;
    for (const auto &[check, index] : integer_guards(clone, profile.size())) {
        if (profile[index] == Representation::small_integer) {
            result.emplace_back(check);
        }
    }
    return result;
}

// Apply a dominating representation proof, using LLVM utilities for the resulting dead operations/branches.
void simplify(llvm::Function &clone, const std::vector<llvm::WeakVH> &checks) {
    for (const auto &handle : checks) {
        auto *check = llvm::dyn_cast_or_null<llvm::Instruction>(static_cast<llvm::Value *>(handle));
        if (!check) {
            continue;
        }
        llvm::replaceAndRecursivelySimplify(check, llvm::ConstantInt::getTrue(clone.getContext()));
        if (auto *remaining = llvm::dyn_cast_or_null<llvm::Instruction>(static_cast<llvm::Value *>(handle))) {
            llvm::RecursivelyDeleteTriviallyDeadInstructions(remaining);
        }
    }
    for (auto &block : clone) {
        llvm::ConstantFoldTerminator(&block, true);
    }
    llvm::removeUnreachableBlocks(clone);
}
} // namespace

llvm::Function *clone_variant(llvm::Function &generic, const TypeProfile &profile) {
    if (profile.empty() || profile.size() > 255) {
        return nullptr;
    }
    llvm::ValueToValueMapTy mapping;
    auto *clone = llvm::CloneFunction(&generic, mapping);
    clone->setName(generic.getName() + ".type");
    clone->setLinkage(llvm::GlobalValue::InternalLinkage);
    const auto checks = proven_checks(*clone, profile);
    if (checks.empty()) {
        clone->eraseFromParent();
        return nullptr;
    }
    simplify(*clone, checks);
    return clone;
}
} // namespace erlang_aot::codegen
