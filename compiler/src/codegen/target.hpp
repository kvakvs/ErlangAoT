#pragma once
#include <string>

namespace erlang_aot::codegen {
class Compilation;

// Resolve the batch target and stamp all modules; errors latch failure without emitting artifacts.
// Repeated calls reuse the machine. Construction alone leaves the compilation incomplete.
bool configure_target(Compilation &compilation);
// Derive the artifact suffix from the configured target format, never the host filesystem.
std::string object_extension(const Compilation &compilation);
} // namespace erlang_aot::codegen
