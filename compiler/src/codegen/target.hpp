#pragma once
#include <string>

namespace clause::codegen {
class Compilation;

// Resolve the batch target and stamp all modules; errors latch failure without emitting artifacts.
// Repeated calls reuse the machine. Construction alone leaves the compilation incomplete.
bool configure_target(Compilation &compilation);
// Derive the artifact suffix from the configured target format, never the host filesystem.
std::string object_extension(const Compilation &compilation);
// Normalize a requested triple; empty selects the running host's triple, as compilation does.
std::string resolve_triple(const std::string &requested);
// Return the configured normalized target triple, which executable linking passes to the Clang driver.
std::string target_triple(const Compilation &compilation);
} // namespace clause::codegen
