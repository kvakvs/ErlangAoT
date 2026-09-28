#include "specialization.hpp"
#include <algorithm>
#include <set>
#include <tuple>

namespace erlang_aot::codegen {
namespace {
struct Budget {
    // Charge additional code independently of the mandatory generic baseline.
    std::size_t remaining = 0;
    std::size_t variants = 0;
};

// Drop unrelated constraints so equivalent observations converge on the same useful profile.
std::size_t canonicalize(TypeProfile &profile, const SpecializationInput &input) {
    std::size_t benefit = 0;
    for (std::size_t i = 0; i < profile.size(); ++i) {
        if (profile[i] == Representation::small_integer && input.checks[i] != 0) {
            benefit += input.checks[i];
        } else {
            profile[i] = Representation::generic;
        }
    }
    return benefit;
}

// Consume two baseline-sized work banks without overflowing their combined size.
struct WorkBudget {
    // Keep two baseline-sized banks together to prevent swapped accounting parameters.
    std::size_t available;
    std::size_t reserve;
};

// Consume bounded analysis work without adding potentially overflowing bank sizes.
bool consume(std::size_t cost, WorkBudget &budget) {
    auto &work = budget.available;
    auto &reserve = budget.reserve;
    if (work >= cost) {
        work -= cost;
        return true;
    }
    const auto needed = cost - work;
    if (reserve < needed) {
        return false;
    }
    reserve -= needed;
    work = 0;
    return true;
}

// Charge each observed profile before inspecting it; exhaustion preserves the generic body.
std::set<TypeProfile> profiles(const SpecializationInput &input, SpecializationPlan &plan) {
    std::set<TypeProfile> result;
    WorkBudget work{input.baseline, input.baseline};
    for (const auto &observed : input.profiles) {
        const auto cost = observed.size() + 1;
        if (!consume(cost, work)) {
            ++plan.decisions[SpecializationReason::work_limit];
            break;
        }
        if (observed.size() != input.checks.size()) {
            continue;
        }
        auto profile = observed;
        if (canonicalize(profile, input) == 0) {
            ++plan.decisions[SpecializationReason::no_benefit];
        } else if (!result.insert(std::move(profile)).second) {
            ++plan.decisions[SpecializationReason::duplicate];
        }
    }
    return result;
}

// Conservatively count a clone with proven checks removed plus all guard/dispatch instructions.
std::size_t estimate(const SpecializationInput &input, TypeProfile profile) {
    const auto benefit = canonicalize(profile, input);
    const auto guarded = static_cast<std::size_t>(std::ranges::count(profile, Representation::small_integer));
    return input.baseline - std::min(input.baseline, benefit) + guarded * 5 + 4;
}

// Admit only useful profiles whose estimated whole-function and whole-module growth both fit.
void select(const SpecializationInput &input, Budget &module, SpecializationPlan &plan) {
    Budget function{input.baseline, 0};
    for (const auto &profile : profiles(input, plan)) {
        if (function.variants == 3 || module.variants == 32 || plan.candidates.size() == 128) {
            ++plan.decisions[SpecializationReason::variant_limit];
            break;
        }
        const auto growth = estimate(input, profile);
        if (growth > function.remaining || growth > module.remaining) {
            ++plan.decisions[SpecializationReason::growth_limit];
            continue;
        }
        function.remaining -= growth;
        module.remaining -= growth;
        ++function.variants;
        ++module.variants;
        ++plan.decisions[SpecializationReason::accepted];
        plan.candidates.push_back({input.module, input.symbol, profile, growth});
    }
}
} // namespace

SpecializationPlan plan_specializations(const CompilationRequest &request,
                                        std::span<const SpecializationInput> inputs) {
    SpecializationPlan result;
    if (request.optimization != OptimizationLevel::speed || request.disable_type_specialization) {
        result.decisions[SpecializationReason::disabled] = inputs.size();
        return result;
    }
    std::vector<const SpecializationInput *> ordered;
    std::map<std::string, Budget> modules;
    for (const auto &input : inputs) {
        ordered.push_back(&input);
        modules[input.module].remaining += input.baseline;
    }
    std::ranges::sort(ordered, [](const auto *left, const auto *right) {
        return std::tie(left->module, left->symbol) < std::tie(right->module, right->symbol);
    });
    for (const auto *input : ordered) {
        select(*input, modules.at(input->module), result);
    }
    return result;
}
} // namespace erlang_aot::codegen
