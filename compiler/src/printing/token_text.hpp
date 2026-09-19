#pragma once
#include <erlang_aot/compiler/lexer.hpp>
#include <span>

namespace erlang_aot {
// Render canonical token values using epp's stringification conventions.
std::u32string token_text(const Token &token);
// Join canonical tokens for macro stringification, not source serialization.
std::u32string stringify(std::span<const Token> tokens);
} // namespace erlang_aot
