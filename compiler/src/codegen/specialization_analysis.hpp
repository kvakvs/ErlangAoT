#pragma once
#include "lowering.hpp"
#include "specialization.hpp"

namespace clause::codegen {
// Collect bounded implementation-only call profiles and measured LLVM check-removal opportunities.
SpecializationPlan analyze_specializations(Compilation &compilation,
                                           std::span<const std::unique_ptr<semantic::Module>> modules,
                                           const semantic::types::Inference &inferred);
} // namespace clause::codegen
