#pragma once
#include "declarations.hpp"
#include "inference.hpp"
#include <erlang_aot/compiler/diagnostic.hpp>

namespace erlang_aot::semantic::types {
// Warn only when every declared alternative provably excludes an inferred singleton.
void check_contracts(Registry &declared, const Inference &inferred, const CallGraph &calls, const Reporter &out);
// Render deterministic, escaped implementation facts for opt-in debug output.
void trace_inference(const Inference &inferred, const CallGraph &calls,
                     const DiagnosticSink &sink, int step = 23);
// Test singleton membership conservatively; unknown structures never prove a discrepancy.
bool excludes_integer(Registry &declared, std::string_view value, Id type, std::string_view owner);
} // namespace erlang_aot::semantic::types
