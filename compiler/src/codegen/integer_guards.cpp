#include "integer_guards.hpp"
#include <llvm/IR/Module.h>
#include <set>

namespace erlang_aot::codegen {
namespace {
using Users = std::set<const llvm::User *>;

// Ask LLVM to compare exact operations, keeping intrusive operand layout inside the SDK implementation.
bool tag_mask(llvm::BinaryOperator &mask, llvm::LoadInst &load) {
    auto *expected = llvm::BinaryOperator::CreateAnd(&load, llvm::ConstantInt::get(load.getType(), 15));
    const auto matches = mask.isIdenticalTo(expected);
    expected->deleteValue();
    return matches;
}

// Match the low-tag equality through LLVM's comparison API; temporary probes never enter a module.
bool tag_comparison(llvm::ICmpInst &check, llvm::BinaryOperator &mask) {
    auto *expected = new llvm::ICmpInst(llvm::CmpInst::ICMP_EQ, &mask, llvm::ConstantInt::get(mask.getType(), 15));
    const auto matches = check.isIdenticalTo(expected);
    expected->deleteValue();
    return matches;
}

// Only entry-block observations before side effects may use an invocation-entry representation proof.
Users safe_users(llvm::Function &function) {
    Users result;
    for (auto &instruction : function.getEntryBlock()) {
        if (instruction.mayHaveSideEffects()) {
            break;
        }
        result.insert(&instruction);
    }
    return result;
}

// Find exact equality users of an argument's low-tag mask through LLVM's stable use lists.
void comparisons(llvm::BinaryOperator &mask, std::size_t index, const Users &safe, IntegerGuards &result) {
    for (const auto &use : mask.uses()) {
        auto *check = llvm::dyn_cast<llvm::ICmpInst>(use.getUser());
        if (check && use.getOperandNo() == 0 && safe.contains(check) && tag_comparison(*check, mask)) {
            result.emplace(check, index);
        }
    }
}

// Require a side-effect-free word load and an exact mask rather than broad integer annotations.
void masks(llvm::LoadInst &load, const std::size_t index, const Users &safe, IntegerGuards &result) {
    for (const auto &use : load.uses()) {
        auto *mask = llvm::dyn_cast<llvm::BinaryOperator>(use.getUser());
        if (mask && use.getOperandNo() == 0 && safe.contains(mask) && tag_mask(*mask, load)) {
            comparisons(*mask, index, safe, result);
        }
    }
}

// Reject volatile/atomic and mismatched-width loads before considering their checks.
void loads(llvm::GetElementPtrInst &slot, const std::size_t index, const Users &safe, IntegerGuards &result) {
    for (const auto &use : slot.uses()) {
        auto *load = llvm::dyn_cast<llvm::LoadInst>(use.getUser());
        if (!load || use.getOperandNo() != 0 || !safe.contains(load)) {
            continue;
        }
        if (!load->isVolatile() && !load->isAtomic() && load->getType() == slot.getSourceElementType()) {
            masks(*load, index, safe, result);
        }
    }
}

// Let LLVM decode a constant byte offset, avoiding host-width assumptions and direct operand inspection.
std::optional<std::size_t> slot_index(llvm::GetElementPtrInst &slot, llvm::Function &function,
                                      const std::size_t arity) {
    if (slot.getNumIndices() != 1 || slot.getSourceElementType() != function.getReturnType()) {
        return {};
    }
    const auto &layout = function.getParent()->getDataLayout();
    const auto bits = layout.getPointerSizeInBits();
    llvm::APInt offset(bits, 0);
    if (!slot.accumulateConstantOffset(layout, offset) || offset.isNegative()) {
        return {};
    }
    const auto bytes = offset.getZExtValue();
    const auto index = bytes / (bits / 8);
    if (bytes % (bits / 8) != 0 || index >= arity) {
        return {};
    }
    return static_cast<std::size_t>(index);
}
} // namespace

IntegerGuards integer_guards(llvm::Function &function, const std::size_t arity) {
    IntegerGuards result;
    if (function.isDeclaration() || function.arg_size() != 2) {
        return result;
    }
    const auto safe = safe_users(function);
    for (const auto &use : function.getArg(1)->uses()) {
        auto *slot = llvm::dyn_cast<llvm::GetElementPtrInst>(use.getUser());
        if (!slot || use.getOperandNo() != 0 || !safe.contains(slot)) {
            continue;
        }
        if (const auto index = slot_index(*slot, function, arity)) {
            loads(*slot, *index, safe, result);
        }
    }
    return result;
}
} // namespace erlang_aot::codegen
