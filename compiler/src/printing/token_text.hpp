#pragma once
#include <erlang_aot/compiler/lexer.hpp>
#include <span>

namespace erlang_aot {
// Render canonical token values using epp's stringification conventions.
std::u32string token_text(const Token &token);
// Share literal escaping and numeric formatting with AST values without source tokens.
std::u32string token_text(TokenKind kind, const TokenValue &value);
// Join canonical tokens for macro stringification, not source serialization.
std::u32string stringify(std::span<const Token> tokens);
} // namespace erlang_aot
