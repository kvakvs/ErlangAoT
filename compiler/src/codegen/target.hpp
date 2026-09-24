#pragma once

namespace erlang_aot::codegen {
class Compilation;

// Resolve the batch target and stamp all modules; errors latch failure without emitting artifacts.
// Repeated calls reuse the machine. Construction alone leaves the compilation incomplete.
bool configure_target(Compilation &compilation);
} // namespace erlang_aot::codegen
