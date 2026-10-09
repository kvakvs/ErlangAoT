#pragma once
#include "inference_bindings.hpp"
#include "lattice.hpp"

// Narrowing by tests and patterns (docs/semantic.md#inference): what a true guard test proves about the variables it
// reads, and the values a pattern can match.
namespace clause::semantic::types {
// Narrow the variables a true test reads (is_* type tests, comparisons of proven integers with integer constants,
// conjunctions and disjunctions of them); false when the test can never be true.
bool assume(BindingFacts &bindings, const ast::ExprId &test);
// Narrow by a true guard: the join of what each alternative proves; false when no alternative can be true.
bool assume_guard(BindingFacts &bindings, const ast::GuardSyntax &guard);
// The values a pattern can match: literals, tuples, lists, records, maps and bitstrings by their shape, a bound
// variable by its fact, a new one any term.
Id pattern_shape(BindingFacts &bindings, const ast::ExprId &pattern);
// The variable and category of a guard that is a single type test on a variable.
std::optional<std::pair<BindingId, Id>> single_test(BindingFacts &bindings, const ast::GuardSyntax &guard);
// The identity a pattern or read of a plain variable names; none for other expressions and for `_`.
std::optional<BindingId> variable(const BindingFacts &bindings, const ast::ExprId &expression);
} // namespace clause::semantic::types
