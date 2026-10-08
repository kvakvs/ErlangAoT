#pragma once
#include <span>

namespace clause::cli {
// Dispatch a complete CLI request; the process entry point owns unexpected failures.
int run_command(std::span<char *> arguments);
} // namespace clause::cli
