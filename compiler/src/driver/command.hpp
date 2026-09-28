#pragma once
#include <span>

namespace erlang_aot::cli {
// Dispatch a complete CLI request; the process entry point owns unexpected failures.
int run_command(std::span<char *> arguments);
} // namespace erlang_aot::cli
