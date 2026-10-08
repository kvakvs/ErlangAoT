#pragma once
#include "../implementation_debug.hpp"
#include <optional>
#include <span>
#include <string>

namespace clause::cli {
// Consume one decimal integer/list operand and merge it only after validating every member.
std::optional<std::string> parse_implementation_debug(std::span<char *> &remaining, ImplementationDebug &selection);
} // namespace clause::cli
