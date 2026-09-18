#pragma once
#include <erlang_aot/compiler/ast/source.hpp>
#include <erlang_aot/compiler/lexer.hpp>

namespace erlang_aot::ast {
struct Atom {
    // Store the decoded atom name without depending on runtime atom tables.
    std::u32string name;
};

struct Variable {
    // Preserve spelling; binding and wildcard interpretation belong to later analysis.
    std::u32string name;
};

struct IntegerLiteral {
    // Reuse arbitrary-precision canonical digits from the existing lexer.
    Integer value;
};

struct FloatLiteral {
    // Preserve the scanner's binary64 value without decimal conversion.
    double value;
};

struct CharacterLiteral {
    // Keep character syntax distinct from an integer literal of the same value.
    char32_t value;
};

struct StringLiteral {
    // Retain decoded Unicode characters, independently of original literal spelling.
    std::u32string value;
};

using ExprValue = std::variant<Atom, Variable, IntegerLiteral, FloatLiteral, CharacterLiteral, StringLiteral>;

struct Expression {
    // Associate a closed, typed payload with its expanded-token extent.
    ExprValue value;
    NodeSource source;
};
} // namespace erlang_aot::ast
