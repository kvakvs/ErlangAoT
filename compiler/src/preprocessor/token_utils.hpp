#pragma once
#include "parsing/token_syntax.hpp"
#include <span>

namespace erlang_aot {
// Create generated tokens while preserving expansion location and source provenance.
Token generated(const Token &origin, TokenKind kind, TokenValue value);
// Render canonical Erlang token text for stringification and diagnostic terms.
std::u32string token_text(const Token &token);
std::u32string stringify(std::span<const Token> tokens);
// Raise a structured semantic error at an invocation and retain its trace.
[[noreturn]] void pp_fail(DiagnosticCode code, std::string message, const Token &token);
// Read a private compiler-generated fragment using the same lexical rules as input.
std::vector<Token> fragment(std::string text);
} // namespace erlang_aot
