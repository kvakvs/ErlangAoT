#pragma once
#include "parsing/token_syntax.hpp"
#include "printing/token_text.hpp"
#include <span>

namespace erlang_aot {
// Create generated tokens while preserving expansion location and source provenance.
Token generated(const Token &origin, TokenKind kind, TokenValue value);
// Raise a structured semantic error at an invocation and retain its trace.
[[noreturn]] void pp_fail(DiagnosticCode code, std::string message, const Token &token);
// Read a private compiler-generated fragment using the same lexical rules as input.
std::vector<Token> fragment(std::string text);
} // namespace erlang_aot
