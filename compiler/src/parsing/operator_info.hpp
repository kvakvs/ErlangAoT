#pragma once
#include <erlang_aot/compiler/ast/operators.hpp>
#include <erlang_aot/compiler/lexer.hpp>

namespace erlang_aot {
enum class Associativity : std::uint8_t { left, right, none };
enum class OperatorContext : std::uint8_t { expression, pattern, condition, type };

struct OperatorInfo {
    // Preserve syntax identity, relative binding strength, and grouping policy.
    std::u32string_view spelling;
    int precedence;
    Associativity associativity;
    // Match, remote qualification and type-only syntax have their own typed payloads.
    std::optional<ast::BinaryOperator> operation = {};
};

struct PrefixOperatorInfo {
    // Prefix operators share precedence but retain a closed syntax identity.
    ast::UnaryOperator operation;
    int precedence = 600;
};

// Recognize category-aware prefix and call syntax independently of infix operators.
std::optional<PrefixOperatorInfo> prefix_operator(const Token &token);
std::optional<OperatorInfo> call_operator(const Token &token);

// Look up infix operators only in the requested grammar, excluding quoted atoms.
std::optional<OperatorInfo> infix_operator(const Token &token, OperatorContext context);
} // namespace erlang_aot
