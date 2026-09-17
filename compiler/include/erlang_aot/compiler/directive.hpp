#pragma once
#include <erlang_aot/compiler/lexer.hpp>
#include <span>

namespace erlang_aot {
enum class DirectiveKind : std::uint8_t {
    define,
    undef,
    include,
    include_lib,
    ifdef,
    ifndef,
    if_condition,
    elif,
    else_branch,
    endif,
    feature,
    error,
    warning
};

struct Definition {
    // Distinguish object macros from parameter lists, including arity zero.
    Token name;
    std::optional<std::vector<Token>> parameters;
    // Preserve arbitrary replacement syntax and its original source spelling.
    std::vector<Token> body;
};

struct MacroName {
    // Retain the operand location for later definedness/undefinition
    // diagnostics.
    Token name;
};

struct TokenOperand {
    // Defer expansion and expression/term validation to their semantic stages.
    std::vector<Token> tokens;
};

struct FeatureSetting {
    // Preserve the requested feature and its explicit enable/disable choice.
    Token name;
    bool enabled;
};

using DirectiveOperand = std::variant<std::monostate, Definition, MacroName, TokenOperand, FeatureSetting>;

struct Directive {
    // Store a fully parsed envelope without applying preprocessing effects.
    DirectiveKind kind;
    DirectiveOperand operand;
    Span spelling;
};

// Recognize only supported names following an attribute dash, including
// keywords.
std::optional<DirectiveKind> directive_kind(std::span<const Token> tokens);
// Parse a complete recognized form transactionally; malformed syntax is a
// value.
std::variant<Directive, Diagnostic> parse_directive(std::span<const Token> tokens);
} // namespace erlang_aot
