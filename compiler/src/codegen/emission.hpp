#pragma once

namespace erlang_aot::codegen {
class Compilation;

// Reverify the whole batch, then replace staged output with native objects; never publish files.
bool emit_objects(Compilation &compilation);
} // namespace erlang_aot::codegen
