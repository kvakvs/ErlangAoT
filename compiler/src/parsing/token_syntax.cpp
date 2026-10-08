#include "token_syntax.hpp"

namespace clause {
bool syntax(const Token &token, const std::u32string_view text) {
    return (token.kind == TokenKind::symbol || token.kind == TokenKind::keyword || token.kind == TokenKind::dot) &&
           token.text() == text;
}

Diagnostic token_diagnostic(const DiagnosticCode code, std::string message, const Token &token) {
    return {.code = code,
            .message = std::move(message),
            .primary = token.spelling,
            .related = token.origins,
            .severity = Severity::error,
            .location = token.location};
}
} // namespace clause
