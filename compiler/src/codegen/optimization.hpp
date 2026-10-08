#pragma once
#include "compilation.hpp"

namespace clause::codegen {
// Verify the batch, run LLVM's selected standard pipeline, then verify its resulting IR.
bool optimize(Compilation &compilation);
} // namespace clause::codegen
