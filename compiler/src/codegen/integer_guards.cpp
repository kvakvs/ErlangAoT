#include "integer_guards.hpp"
#include <llvm/IR/Module.h>

namespace clause::codegen {
namespace {
// Ask LLVM to compare exact operations, keeping intrusive operand layout inside the SDK implementation.
bool tag_mask(llvm::BinaryOperator &mask, llvm::LoadInst &load) {
    auto *expected = llvm::BinaryOperator::CreateAnd(&load, llvm::ConstantInt::get(load.getType(), 15));
    const auto matches = mask.isIdenticalTo(expected);
    expected->deleteValue();
    return matches;
}

// Match the low-tag equality through LLVM's comparison API; temporary probes never enter a module.
bool tag_comparison(llvm::ICmpInst &check, llvm::BinaryOperator &mask) {
    auto *expected = llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::CmpInst::ICMP_EQ, &mask,
                                           llvm::ConstantInt::get(mask.getType(), 15));
    const auto matches = check.isIdenticalTo(expected);
    expected->deleteValue();
    return matches;
}

// Find exact equality users of an argument's low-tag mask through LLVM's stable use lists.
void comparisons(llvm::BinaryOperator &mask, std::size_t index, IntegerGuards &result) {
    for (const auto &use : mask.uses()) {
        auto *check = llvm::dyn_cast<llvm::ICmpInst>(use.getUser());
        if (check && use.getOperandNo() == 0 && tag_comparison(*check, mask)) {
            result.emplace(check, index);
        }
    }
}

// Require an exact mask of an argument word rather than broad integer annotations.
void masks(llvm::LoadInst &load, const std::size_t index, IntegerGuards &result) {
    for (const auto &use : load.uses()) {
        auto *mask = llvm::dyn_cast<llvm::BinaryOperator>(use.getUser());
        if (mask && use.getOperandNo() == 0 && tag_mask(*mask, load)) {
            comparisons(*mask, index, result);
        }
    }
}

// Reject volatile/atomic and mismatched-width loads before considering their checks. The argument array is never
// written and a collection never changes a small integer, so a load anywhere in the body reads the entry tag.
void loads(llvm::GetElementPtrInst &slot, const std::size_t index, IntegerGuards &result) {
    for (const auto &use : slot.uses()) {
        auto *load = llvm::dyn_cast<llvm::LoadInst>(use.getUser());
        if (load && use.getOperandNo() == 0 && !load->isVolatile() && !load->isAtomic() &&
            load->getType() == slot.getSourceElementType()) {
            masks(*load, index, result);
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
    for (const auto &use : function.getArg(1)->uses()) {
        auto *slot = llvm::dyn_cast<llvm::GetElementPtrInst>(use.getUser());
        if (!slot || use.getOperandNo() != 0) {
            continue;
        }
        if (const auto index = slot_index(*slot, function, arity)) {
            loads(*slot, *index, result);
        }
    }
    return result;
}
} // namespace clause::codegen
