#pragma once
#include <clause/compiler/ast/expressions.hpp>

namespace clause::semantic {
enum class PatternKind : std::uint8_t {
    variable,
    wildcard,
    literal,
    tuple,
    list,
    alias,
    map,
    bitstring,
    record,
    record_index,
    prefix
};
using PatternLiteral = std::variant<ast::IntegerLiteral, ast::FloatLiteral, ast::Atom, ast::StringLiteral>;

struct NormalizedPattern {
    // Keep both the original anchor and ungrouped identity; children still refer to the owned AST.
    ast::ExprId origin;
    ast::ExprId expression;
    PatternKind kind;
    std::vector<ast::ExprId> children;
    // Folded constants are owned, target-independent values; no runtime representation is implied.
    std::optional<PatternLiteral> literal = {};
};
} // namespace clause::semantic
