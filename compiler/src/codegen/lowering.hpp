#pragma once
#include "../semantic/types/inference.hpp"
#include "compilation.hpp"

namespace clause::codegen {
// Lower a validated batch in request order; analysis and syntax must outlive this call.
// Keep every entry generic even when inference or declarations describe narrower types.
bool lower(Compilation &compilation, std::span<const std::unique_ptr<semantic::Module>> modules,
           const semantic::types::Inference &inferred);
} // namespace clause::codegen
