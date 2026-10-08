#pragma once
#include "request.hpp"
#include <span>

namespace clause::codegen {
// Check retained syntax plus an optional new module before adding it to a frontend batch.
void validate_input_limits(std::span<const CompilationInput> inputs, const CompilationLimits &limits = {},
                           const ast::Module *additional = nullptr);
// Return the remaining bounded capacity for the next artifact or inspection snapshot.
std::size_t output_capacity(const CompilationLimits &limits, std::span<const OutputBuffer> outputs);
} // namespace clause::codegen
