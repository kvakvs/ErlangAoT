#pragma once
#include "analysis.hpp"

namespace clause::cli {
// Print deterministic declaration/function/expression summaries and stop before constructing LLVM state.
void print_types(const Analysis &analysis, const codegen::CompilationRequest &request);
} // namespace clause::cli
