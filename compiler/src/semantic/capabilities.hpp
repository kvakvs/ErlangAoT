#pragma once
#include "declarations.hpp"

namespace erlang_aot::semantic {
struct Branch {
    // One case, if or try clause seen uniformly: if clauses have no pattern, case clauses may omit the guard.
    const ast::PatternSyntaxId *pattern;
    const ast::GuardSyntax *guard;
    const std::vector<ast::ExprId> *body;
    // A try catch clause also matches the exception class; its pattern is the reason (null for other clauses).
    const ast::CatchClause *handler = nullptr;
};

// List the clauses of a case or if expression, a try's of then catch clauses, or a maybe's else clauses, in source
// order; every other expression has none.
std::vector<Branch> branch_clauses(const ast::ExprValue &value);
// Index of a try's first catch clause in branch_clauses (0 for a maybe's else clauses, the clause count for case
// and if).
std::size_t first_handler(const ast::ExprValue &value);
// The expressions a maybe body evaluates in order: plain expressions and the values of its ?= matches.
std::vector<ast::ExprId> maybe_operands(const ast::MaybeExpression &value);
// Reject executable syntax outside the milestone without modifying parser coverage.
void check_capabilities(const Module &module, const Reporter &out, unsigned word_bits = sizeof(void *) * 8);
// Iterate accepted expression children without visiting literal call-target atoms as values.
std::vector<ast::ExprId> expression_children(const ast::Expression &expression);
// Include selected record defaults in the same bounded executable walks as explicit operands.
std::vector<ast::ExprId> expression_children(const Module &module, const ast::Expression &expression);
// Return guard/body roots in source order across every candidate; heads use normalized pattern plans.
std::vector<ast::ExprId> function_roots(const ast::Function &function);
// Enumerate analyzed map key expressions independently of value-pattern definitions.
std::vector<ast::ExprId> pattern_reads(const Module &module, const Function &function);
// Return the expression a restricted or permissive pattern syntax node wraps.
ast::ExprId pattern_root(const ast::Module &syntax, const ast::PatternSyntaxId &pattern);
// Strip source-only grouping while retaining the original node for diagnostics.
ast::ExprId ungroup(const ast::Module &syntax, ast::ExprId expression);
// Decode supported signed literals using the eventual target word width, never unchecked casts.
std::optional<std::int64_t> integer_literal(const ast::Module &syntax, ast::ExprId expression, unsigned word_bits);
} // namespace erlang_aot::semantic
