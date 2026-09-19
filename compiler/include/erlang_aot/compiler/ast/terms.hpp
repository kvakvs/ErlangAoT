#pragma once
#include <erlang_aot/compiler/ast/expressions.hpp>

namespace erlang_aot::ast {
struct TermTuple {
    // Literal children cannot refer to executable expressions.
    std::vector<TermId> elements;
};

struct TermList {
    // Flatten proper list spines while preserving an optional improper tail.
    std::vector<TermId> elements;
    std::optional<TermId> tail;
};

struct TermMap {
    // Preserve normalized exact keys and their final values.
    std::vector<std::pair<TermId, TermId>> entries;
};

struct TermBits {
    // Own literal bits, including a partial final byte, independently of runtime layout.
    std::vector<bool> bits;
};

struct TermFunction {
    // External fun literals are data; parsing never resolves or invokes their target.
    Atom module;
    Atom name;
    Integer arity;
};

using TermValue =
    std::variant<Atom, IntegerLiteral, FloatLiteral, TermTuple, TermList, TermMap, TermBits, TermFunction>;

struct LiteralTerm {
    // Normalized attribute data retains the original expression's source extent.
    TermValue value;
    NodeSource source;
};
} // namespace erlang_aot::ast
