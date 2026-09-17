#pragma once
#include <erlang_aot/compiler/directive.hpp>

namespace erlang_aot {
// A bounded token cursor keeps lexical categories intact during envelope parsing.
class DirectiveCursor {
public:
    // Retain an end sentinel for useful locations when an operand is missing.
    DirectiveCursor(std::span<const Token> tokens, Span end) : tokens_(tokens), end_(std::move(end)) {}
    // Consume punctuation only, never a quoted atom with identical decoded text.
    bool take(std::u32string_view symbol)
    {
        if (tokens_.empty()) { return false; }
        if (tokens_.front().kind != TokenKind::symbol || tokens_.front().text() != symbol) { return false; }
        tokens_ = tokens_.subspan(1);
        return true;
    }
    // Require a delimiter while retaining the first unexpected token's location.
    void expect(std::u32string_view symbol)
    {
        if (!take(symbol)) { fail("expected '" + utf8(symbol) + "'"); }
    }
    // Consume a macro name (atom or variable), or a narrower lexical category.
    Token name()
    {
        if (tokens_.empty()) { fail("expected macro name"); }
        if (tokens_.front().kind == TokenKind::atom) { return category(TokenKind::atom, "atom"); }
        return category(TokenKind::variable, "macro name");
    }
    Token category(TokenKind kind, std::string_view description)
    {
        if (tokens_.empty()) { fail("expected " + std::string(description)); }
        if (tokens_.front().kind != kind) { fail("expected " + std::string(description)); }
        auto result = tokens_.front();
        tokens_ = tokens_.subspan(1);
        return result;
    }
    // Copy replacement/condition tokens without interpreting Erlang expressions.
    std::vector<Token> remainder() const { return {tokens_.begin(), tokens_.end()}; }
    // Reject extra operands for directives with a fixed envelope.
    void finish() const { if (!tokens_.empty()) { fail("unexpected directive operand"); } }
    // Throw only within the transactional parser, which returns structured errors.
    [[noreturn]] void fail(std::string message) const
    {
        throw Diagnostic{DiagnosticCode::malformed_directive, std::move(message),
            tokens_.empty() ? end_ : tokens_.front().spelling, {}};
    }
private:
    // Remaining borrowed tokens and the closing delimiter's owned source span.
    std::span<const Token> tokens_;
    Span end_;
};
} // namespace erlang_aot
