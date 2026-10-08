#pragma once
#include "declarations.hpp"

namespace clause::semantic {
// Apply escript rules: validate -mode attributes and implicitly export the required main/1.
void index_escript(Module &module, const Reporter &out);
} // namespace clause::semantic
