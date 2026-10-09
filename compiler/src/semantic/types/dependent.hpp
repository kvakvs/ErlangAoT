#pragma once
#include "inference_bindings.hpp"

// Dependent facts (docs/semantic.md#dependent-facts): the value of a case or if as a function of the variables its
// clauses narrow, one function type per possible clause, like a fun applied to those variables.
namespace clause::semantic::types {
// Parameters a dependent fact keeps at most; later ones are dropped and their narrowing forgotten.
inline constexpr std::size_t DEPENDENT_PARAMETERS = 4;

// The fact without its dependence: what is kept beyond the walked function.
Fact erased(Fact fact);
// After a possible clause's guard: keep the facts of its case's or if's parameters entering it.
void record_key(BindingFacts &bindings, const ast::Expression &construct, std::size_t index);
// The value of a case or if from its clauses' joined value: dependent on its parameters when its clauses give
// different values for different parameter facts.
Fact dependent_value(BindingFacts &bindings, const ast::Expression &construct, Fact joined);
// A fact as read where `values` hold: a dependent fact keeps the clauses its parameters' facts enter, and its type
// narrows to their results.
Fact resolved(Inference &inference, Fact fact, const std::map<BindingId, Fact> &values);
// A function clause's function types from its result: one per clause of a dependent result whose parameters name
// arguments, their facts met with the arguments'; else one.
std::vector<FunctionType> clause_types(const BindingFacts &bindings, const std::vector<ast::ExprId> &patterns,
                                       Fact result);
} // namespace clause::semantic::types
