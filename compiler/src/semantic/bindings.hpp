#pragma once
#include "declarations.hpp"

namespace erlang_aot::semantic {
// Resolve each body variable to its original argument position without modifying syntax.
void bind_parameters(Module &module, const Reporter &out);
} // namespace erlang_aot::semantic
