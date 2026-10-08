#pragma once
#include "../codegen/request.hpp"
#include "frontend.hpp"

namespace clause::cli {
// Own and compile a parsed batch; return failure only after retaining/reporting its diagnostics.
bool compile_batch(std::vector<codegen::CompilationInput> inputs, const FrontendRequest &request,
                   const DiagnosticSink &sink);
} // namespace clause::cli
