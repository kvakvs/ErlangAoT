#pragma once
#include "parsing/token_cursor.hpp"

namespace erlang_aot {
// A bounded token cursor keeps lexical categories intact during envelope
// parsing.
class DirectiveCursor {
  public:
    // Retain an end sentinel for useful locations when an operand is missing.
    DirectiveCursor(std::span<const Token> tokens, Span end)
        : cursor_(tokens, Token{TokenKind::dot, std::u32string{}, std::move(end), {}, {}}) {}

    // Consume punctuation only, never a quoted atom with identical decoded
    // text.
    bool take(std::u32string_view symbol) { return cursor_.take(TokenKind::symbol, symbol); }

    // Require a delimiter while retaining the first unexpected token's
    // location.
    void expect(std::u32string_view symbol) {
        if (!take(symbol)) {
            fail("expected '" + utf8(symbol) + "'");
        }
    }

    // Consume a macro name (atom or variable), or a narrower lexical category.
    Token name() {
        if (cursor_.empty()) {
            fail("expected macro name");
        }
        if (cursor_.anchor().kind == TokenKind::atom) {
            return category(TokenKind::atom, "atom");
        }
        return category(TokenKind::variable, "macro name");
    }

    Token category(TokenKind kind, std::string_view description) {
        if (cursor_.empty()) {
            fail("expected " + std::string(description));
        }
        if (cursor_.anchor().kind != kind) {
            fail("expected " + std::string(description));
        }
        auto result = cursor_.anchor();
        cursor_.consume();
        return result;
    }

    // Copy replacement/condition tokens without interpreting Erlang
    // expressions.
    std::vector<Token> remainder() const { return {cursor_.remaining().begin(), cursor_.remaining().end()}; }

    // Reject extra operands for directives with a fixed envelope.
    void finish() const {
        if (!cursor_.empty()) {
            fail("unexpected directive operand");
        }
    }

    // Throw only within the transactional parser, which returns structured
    // errors.
    [[noreturn]] void fail(std::string message) const {
        throw Diagnostic{DiagnosticCode::malformed_directive,
                         std::move(message),
                         cursor_.anchor().spelling,
                         {},
                         Severity::error,
                         cursor_.empty() ? std::nullopt : std::optional(cursor_.anchor().location)};
    }

  private:
    // Remaining borrowed tokens and the closing delimiter's owned source span.
    TokenCursor cursor_;
};
} // namespace erlang_aot
