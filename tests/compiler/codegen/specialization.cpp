#include "codegen/specialization.hpp"
#include "lowering_support.hpp"

using R = cg::Representation;

// Model measured repeated checks to exercise limits absent from the current guard-free source subset.
cg::SpecializationInput useful(std::string module, std::string symbol) {
    return {std::move(module),
            std::move(symbol),
            1000,
            {850, 30, 30, 30},
            {{R::small_integer, R::generic, R::generic, R::generic},
             {R::small_integer, R::small_integer, R::generic, R::generic},
             {R::small_integer, R::generic, R::small_integer, R::generic},
             {R::small_integer, R::generic, R::generic, R::small_integer}}};
}

// Pin deterministic hard caps and deduplication, including reversed input ordering.
void budgets() {
    cg::CompilationRequest request;
    request.optimization = cg::OptimizationLevel::speed;
    std::vector<cg::SpecializationInput> inputs;
    for (unsigned module = 0; module < 5; ++module) {
        for (unsigned function = 0; function < 12; ++function) {
            inputs.push_back(useful(std::to_string(module), std::to_string(function)));
        }
    }
    const auto plan = cg::plan_specializations(request, inputs);
    require(plan.candidates.size() == 128, "target cap not enforced");
    std::map<std::string, unsigned> modules;
    std::map<std::pair<std::string, std::string>, unsigned> functions;
    for (const auto &candidate : plan.candidates) {
        require(++modules[candidate.module] <= 32, "module cap not enforced");
        require(++functions[{candidate.module, candidate.symbol}] <= 3, "function cap not enforced");
    }
    std::ranges::reverse(inputs);
    const auto reversed = cg::plan_specializations(request, inputs);
    for (std::size_t i = 0; i < plan.candidates.size(); ++i) {
        require(plan.candidates[i].symbol == reversed.candidates[i].symbol &&
                    plan.candidates[i].profile == reversed.candidates[i].profile &&
                    plan.candidates[i].module == reversed.candidates[i].module,
                "nondeterministic ranking");
    }
}

// Reject unprofitable growth, deduplicate representation-equivalent sites and bound adversarial profiles.
void policies() {
    cg::CompilationRequest request;
    std::vector inputs{useful("module", "function")};
    require(cg::plan_specializations(request, inputs).candidates.empty(), "O0 specialized");
    request.optimization = cg::OptimizationLevel::speed;
    request.disable_type_specialization = true;
    require(cg::plan_specializations(request, inputs).candidates.empty(), "override ignored");
    request.disable_type_specialization = false;
    const auto repeated_profile = inputs.front().profiles.front();
    inputs.front().profiles.assign(5000, repeated_profile);
    const auto repeated = cg::plan_specializations(request, inputs);
    require(repeated.candidates.size() == 1 && repeated.decisions.contains(cg::SpecializationReason::duplicate) &&
                repeated.decisions.contains(cg::SpecializationReason::work_limit),
            "profile work not bounded");
    inputs.front().checks.assign(4, 1);
    require(cg::plan_specializations(request, inputs).candidates.empty(), "growth estimate ignored dispatch");
    inputs.front().checks.assign(255, 0);
    inputs.front().profiles.assign(5000, cg::TypeProfile(255, R::generic));
    require(cg::plan_specializations(request, inputs).candidates.empty(), "unknown union product enumerated");
}

// Current supported programs have no representation checks, so speed mode must not invent variants.
void real_sources() {
    auto compilation = fixtures({"client.erl", "answer.erl"}, "", cg::OptimizationLevel::speed);
    require(analyze_and_lower(compilation), "speed source lowering failed");
    const auto &plan = cg::detail::state(compilation).specializations;
    require(plan.candidates.empty() && plan.decisions.contains(cg::SpecializationReason::no_benefit),
            "identity/constant cloned without benefit");
}

// Validate the policy layer and real inference boundary without requiring future source operations.
int main() {
    try {
        budgets();
        policies();
        real_sources();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
