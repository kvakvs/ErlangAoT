#include "token_utils.hpp"

namespace clause {
Token generated(const Token &origin, const TokenKind kind, TokenValue value) {
    Token result = origin;
    result.kind = kind;
    result.value = std::move(value);
    return result;
}

void pp_fail(const DiagnosticCode code, std::string message, const Token &token) {
    throw DiagnosticError(token_diagnostic(code, std::move(message), token));
}

std::vector<Token> fragment(std::string text) {
    SourceManager sources;
    Lexer lexer(sources.add("<preprocessor>", std::move(text)));
    std::vector<Token> result;
    while (auto item = lexer.next()) {
        result.push_back(std::move(*item));
    }
    return result;
}
} // namespace clause
