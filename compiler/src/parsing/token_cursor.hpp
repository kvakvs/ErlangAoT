#pragma once
#include "token_syntax.hpp"
#include <span>

namespace erlang_aot {
// Borrow stable tokens while owning the EOF location; grammar-specific errors stay outside.
class TokenCursor {
  public:
    // Keep the supplied EOF anchor usable even for an empty token range.
    TokenCursor(std::span<const Token> tokens, Token end);
    // Inspect or consume without stepping past the supplied range.
    const Token *peek(std::size_t lookahead = 0) const;
    const Token *consume();
    bool take(TokenKind kind, std::u32string_view text);
    bool take_syntax(std::u32string_view text);
    // Save/restore a bounded offset; marks are meaningful only for this cursor.
    std::size_t checkpoint() const;
    void restore(std::size_t offset);
    std::size_t offset() const;
    bool empty() const;
    std::span<const Token> remaining() const;
    // Locate the next token, or use the explicit caller-provided EOF anchor.
    const Token &anchor() const;

  private:
    // Input is borrowed for the cursor lifetime; offsets never exceed its size.
    std::span<const Token> tokens_;
    std::size_t offset_ = 0;
    // EOF carries source ownership independently of the borrowed input.
    Token end_;
};
} // namespace erlang_aot
