#pragma once
#include "compilation.hpp"
#include <optional>

namespace clause::codegen {
// Capture verified assembly without replacing staged artifacts or changing compilation state.
std::optional<std::vector<OutputBuffer>> snapshot_ir(Compilation &compilation);
// Replace staged outputs with verified LLVM assembly or bitcode; object emission stays separate.
bool emit_ir(Compilation &compilation, OutputKind kind);
} // namespace clause::codegen
