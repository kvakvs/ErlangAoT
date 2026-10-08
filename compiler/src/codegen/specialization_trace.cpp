#include "specialization_trace.hpp"
#include "progress.hpp"
#include <algorithm>
#include <array>

namespace clause::codegen {
namespace {
// Bound trace work even for synthetic profiles rejected before the planner can inspect their elements.
std::string profile_text(const TypeProfile &profile) {
    std::string text = "[";
    const auto size = std::min<std::size_t>(profile.size(), 16);
    for (std::size_t i = 0; i < size; ++i) {
        if (i != 0) {
            text += ',';
        }
        text += profile[i] == Representation::small_integer ? "small-integer" : "generic";
    }
    if (profile.size() > size) {
        text += ",...";
    }
    return text + ']';
}
} // namespace

void record_decision(const CompilationRequest &request, SpecializationPlan &plan, const SpecializationInput &input,
                     const TypeProfile &profile, SpecializationReason reason) {
    ++plan.decisions[reason];
    if (!request.progress) {
        return;
    }
    constexpr std::array names{"accepted-benefit", "disabled-by-policy", "no-benefit",   "duplicate-profile",
                               "work-limit",       "growth-limit",       "variant-limit"};
    const auto decision = reason == SpecializationReason::accepted ? " planned=" : " skipped=";
    progress(request, "specialization", input.source_path, input.module,
             "function=" + input.symbol + " profile=" + profile_text(profile) + decision +
                 names.at(static_cast<std::size_t>(reason)));
}
} // namespace clause::codegen
