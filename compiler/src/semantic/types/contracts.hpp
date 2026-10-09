#pragma once
#include "declarations.hpp"
#include "inference.hpp"
#include <clause/compiler/diagnostic.hpp>

namespace clause::semantic::types {
// Report an error for each specification that shares no value with what inference proves: a function's result,
// its entry domain, or a call's arguments (docs/semantic.md#inference).
void check_contracts(Registry &declared, Inference &inferred, const CallGraph &calls, const Reporter &out);
// Render deterministic, escaped implementation facts for opt-in debug output.
void trace_inference(const Inference &inferred, const CallGraph &calls, const DiagnosticSink &sink, int step = 23);
} // namespace clause::semantic::types
