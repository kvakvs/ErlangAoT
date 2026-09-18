#pragma once
#include <erlang_aot/compiler/lexer.hpp>

namespace erlang_aot {
enum class Associativity : std::uint8_t { left, right, none };
enum class OperatorContext : std::uint8_t { expression, condition, type };

struct OperatorInfo {
    // Preserve syntax identity, relative binding strength, and grouping policy.
    std::u32string_view spelling;
    int precedence;
    Associativity associativity;
};

// Look up infix operators only in the requested grammar, excluding quoted atoms.
std::optional<OperatorInfo> infix_operator(const Token &token, OperatorContext context);
} // namespace erlang_aot
