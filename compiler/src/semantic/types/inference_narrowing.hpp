#pragma once
#include "../../preprocessor/value.hpp"
#include "inference_bindings.hpp"
#include "lattice.hpp"

// Narrowing by tests and patterns (docs/semantic.md#inference): what a true guard test proves about the variables it
// reads, and the values a pattern can match.
namespace clause::semantic::types {
// A comparison of a variable with an integer constant, the variable on the left.
struct Comparison {
    BindingId identity;
    BigInt constant;
    ast::BinaryOperator operation;
};

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
// The comparison of a guard that is a single comparison of a variable with an integer constant.
std::optional<Comparison> single_comparison(BindingFacts &bindings, const ast::GuardSyntax &guard);
// Narrow `target`, proven to be an integer, by `compared` (on another name of its value) being false.
void assume_false(BindingFacts &bindings, const Comparison &compared, BindingId target);
// The identity a pattern or read of a plain variable names; none for other expressions and for `_`.
std::optional<BindingId> variable(const BindingFacts &bindings, const ast::ExprId &expression);
} // namespace clause::semantic::types
