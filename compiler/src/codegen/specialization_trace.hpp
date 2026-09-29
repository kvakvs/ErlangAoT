#pragma once
#include "specialization.hpp"

namespace erlang_aot::codegen {
// Count policy decisions and optionally report their bounded profile and concrete rejection reason.
void record_decision(const CompilationRequest &request, SpecializationPlan &plan, const SpecializationInput &input,
                     const TypeProfile &profile, SpecializationReason reason);
} // namespace erlang_aot::codegen
