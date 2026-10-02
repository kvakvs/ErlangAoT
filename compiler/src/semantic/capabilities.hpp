#pragma once
#include "declarations.hpp"

namespace erlang_aot::semantic {
// Reject executable syntax outside the milestone without modifying parser coverage.
void check_capabilities(const Module &module, const Reporter &out, unsigned word_bits = sizeof(void *) * 8);
// Iterate accepted expression children without visiting literal call-target atoms as values.
std::vector<ast::ExprId> expression_children(const ast::Expression &expression);
// Return guard/body roots in source order across every candidate; heads use normalized pattern plans.
std::vector<ast::ExprId> function_roots(const ast::Function &function);
// Strip source-only grouping while retaining the original node for diagnostics.
ast::ExprId ungroup(const ast::Module &syntax, ast::ExprId expression);
// Decode supported signed literals using the eventual target word width, never unchecked casts.
std::optional<std::int64_t> integer_literal(const ast::Module &syntax, ast::ExprId expression, unsigned word_bits);
} // namespace erlang_aot::semantic
