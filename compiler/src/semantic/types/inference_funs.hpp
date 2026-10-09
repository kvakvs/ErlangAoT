#pragma once
#include "inference.hpp"
#include "lattice.hpp"
#include <optional>

// Facts of fun values and of calls of values (docs/semantic.md#inference).
namespace clause::semantic::types {
// The fact of fun F/A (its arity and the function's inferred result) or fun M:F/A (its arity, any result); none for
// another expression.
std::optional<Id> fun_reference_fact(Inference &inference, FunctionRef function, const ast::Expression &expression);
// The fact of a fun of a function: its function types, else a fun of `arity` returning its result.
Id summary_fun(Lattice &lattice, const Summary &summary, std::size_t arity);
// The result of calling a value of fact `callee` with `arity` arguments: the joined results of its funs of that
// arity, term() for an unknown fun, none() when no member can be called so.
Id call_value(Lattice &lattice, Id callee, std::size_t arity);
} // namespace clause::semantic::types
