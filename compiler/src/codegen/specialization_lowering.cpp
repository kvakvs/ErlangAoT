#include "specialization_lowering.hpp"

namespace erlang_aot::codegen {
namespace {
struct ModuleBudget {
    // Limit total added pre-optimization IR and committed variants in this module.
    std::size_t remaining = 0;
    std::size_t variants = 0;
};

struct Draft {
    // Keep probes unpublished until measured clones plus dispatch fit both original baselines.
    std::vector<SpecializedVariant> variants;
    SpecializedDispatch dispatch{nullptr, nullptr};
    std::size_t growth = 0;
};

// Include all clone bodies and every dispatch/fallback instruction in the actual growth measurement.
std::size_t growth(const Draft &draft, const SpecializedDispatch &dispatch) {
    std::size_t result = dispatch.function->getInstructionCount();
    for (const auto &variant : draft.variants) {
        result += variant.function->getInstructionCount();
    }
    return result;
}

// Keep the last fitting draft intact when a later candidate exceeds its measured budget.
bool try_variant(llvm::Function &generic, const SpecializationCandidate &candidate, const ModuleBudget &budget,
                 Draft &draft) {
    auto *clone = clone_variant(generic, candidate.profile);
    if (!clone) {
        return false;
    }
    draft.variants.push_back({clone, candidate.profile});
    const auto dispatch = create_dispatch(generic, draft.variants);
    const auto measured = growth(draft, dispatch);
    if (measured > generic.getInstructionCount() || measured > budget.remaining) {
        dispatch.function->eraseFromParent();
        draft.variants.pop_back();
        clone->eraseFromParent();
        return false;
    }
    if (draft.dispatch.function) {
        draft.dispatch.function->eraseFromParent();
    }
    draft.dispatch = dispatch;
    draft.growth = measured;
    return true;
}

// Preserve the external identity and descriptor references while retaining the original generic implementation.
void install(llvm::Function &generic, Draft &draft) {
    auto *entry = draft.dispatch.function;
    const auto name = generic.getName().str();
    entry->setLinkage(generic.getLinkage());
    entry->setVisibility(generic.getVisibility());
    generic.setName(name + ".generic");
    entry->setName(name);
    generic.replaceUsesWithIf(
        entry, [fallback = draft.dispatch.fallback](llvm::Use &use) { return use.getUser() != fallback; });
    generic.setLinkage(llvm::GlobalValue::InternalLinkage);
}

// Defend hard caps again at emission, including callers of this private stage adapter.
void lower_function(llvm::Function &function, const std::span<const SpecializationCandidate *const> candidates,
                    ModuleBudget &budget, SpecializationPlan &plan) {
    Draft draft;
    for (const auto *candidate : candidates) {
        if (draft.variants.size() == 3 || budget.variants + draft.variants.size() == 32 ||
            plan.lowered_variants + draft.variants.size() == 128) {
            break;
        }
        if (!try_variant(function, *candidate, budget, draft)) {
            ++plan.rejected_variants;
        }
    }
    if (draft.variants.empty()) {
        return;
    }
    install(function, draft);
    budget.remaining -= draft.growth;
    budget.variants += draft.variants.size();
    plan.lowered_variants += draft.variants.size();
}
} // namespace

void lower_specializations(llvm::Module &module, SpecializationPlan &plan) {
    ModuleBudget budget;
    for (const auto &function : module) {
        budget.remaining += function.getInstructionCount();
    }
    std::map<std::string, std::vector<const SpecializationCandidate *>> groups;
    for (const auto &candidate : plan.candidates) {
        groups[candidate.symbol].push_back(&candidate);
    }
    for (const auto &[symbol, candidates] : groups) {
        auto *function = module.getFunction(symbol);
        if (function && !function->isDeclaration()) {
            lower_function(*function, candidates, budget, plan);
        }
    }
}
} // namespace erlang_aot::codegen
