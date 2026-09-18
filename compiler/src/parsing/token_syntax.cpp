#include "token_syntax.hpp"

namespace erlang_aot {
bool syntax(const Token &token, std::u32string_view text) {
    return (token.kind == TokenKind::symbol || token.kind == TokenKind::keyword || token.kind == TokenKind::dot) &&
           token.text() == text;
}

Diagnostic token_diagnostic(DiagnosticCode code, std::string message, const Token &token) {
    return {code, std::move(message), token.spelling, token.origins, Severity::error, token.location};
}
} // namespace erlang_aot
