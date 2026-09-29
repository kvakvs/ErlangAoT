#pragma once
#include "compilation.hpp"

namespace erlang_aot::codegen {
// Verify the batch, run LLVM's selected standard pipeline, then verify its resulting IR.
bool optimize(Compilation &compilation);
} // namespace erlang_aot::codegen
