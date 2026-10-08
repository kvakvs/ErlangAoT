#pragma once
#include <clause/compiler/lexer.hpp>

namespace clause {
// Match syntax categories without interpreting quoted atoms as punctuation or keywords.
bool syntax(const Token &token, std::u32string_view text);
// Construct one diagnostic without losing logical coordinates or expansion origins.
Diagnostic token_diagnostic(DiagnosticCode code, std::string message, const Token &token);
} // namespace clause
