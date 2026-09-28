#pragma once
#include "request.hpp"
#include <map>
#include <span>

namespace erlang_aot::codegen {
enum class Representation : std::uint8_t { generic, small_integer };
using TypeProfile = std::vector<Representation>;
enum class SpecializationReason : std::uint8_t {
    accepted,
    disabled,
    no_benefit,
    duplicate,
    work_limit,
    growth_limit,
    variant_limit
};

struct SpecializationInput {
    // Stable identities determine ranking independently of pointer addresses or input order.
    std::string module;
    std::string symbol;
    // Count the original pre-optimization instructions and removable checks for each argument.
    std::size_t baseline = 0;
    std::vector<std::size_t> checks;
    // Retain observed implementation profiles only; never enumerate unions or literal values.
    std::vector<TypeProfile> profiles;
};

struct SpecializationCandidate {
    // Preserve generic symbol identity and only representation constraints that remove checks.
    std::string module;
    std::string symbol;
    TypeProfile profile;
    // Bound all additional instructions, including runtime dispatch and generic fallback.
    std::size_t growth = 0;
};

struct SpecializationPlan {
    // Own selected profiles and bounded aggregate decisions for future compiler tracing.
    std::vector<SpecializationCandidate> candidates;
    std::map<SpecializationReason, std::size_t> decisions;
};

// Plan deterministically within 3/function, 32/module, 128/target and 2x instruction/work budgets.
SpecializationPlan plan_specializations(const CompilationRequest &request, std::span<const SpecializationInput> inputs);
} // namespace erlang_aot::codegen
