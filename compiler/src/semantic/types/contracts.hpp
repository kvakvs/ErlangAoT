#pragma once
#include "declarations.hpp"
#include "inference.hpp"

namespace clause::semantic::types {
// Report an error for each specification that shares no value with what inference proves: a function's result,
// its entry domain, or a call's arguments (docs/semantic.md#inference).
void check_contracts(Registry &declared, Inference &inferred, const CallGraph &calls, const Reporter &out);
} // namespace clause::semantic::types
