#pragma once
#include "token_syntax.hpp"
#include <span>

namespace erlang_aot {
// Share delimiter/fun-prefix recognition between macro arguments and parser diagnostics.
class Delimiters {
  public:
    // Resolve ambiguous fun prefixes before checking macro-argument boundaries.
    bool boundary(const Token &token);
    // Track the current token with lookahead borrowed from the same immutable form.
    void consume(std::span<const Token> input);
    // Return the nearest unmatched opener, or null outside a construct.
    const Token *opener() const;

  private:
    struct Frame {
        // Borrow tokens only while the caller's immutable form remains alive.
        std::u32string_view close;
        const Token *open;
    };

    // Store open constructs iteratively, without owning or copying source tokens.
    std::vector<Frame> stack_;
    // Distinguish named/anonymous fun bodies from external/local references and fun types.
    void fun(std::span<const Token> input);
};
} // namespace erlang_aot
