#pragma once
#include "inference.hpp"
#include <optional>

// Facts of operator and builtin results (docs/semantic.md#inference), from the facts already recorded for their
// operands: folded constants, interval arithmetic, booleans of comparisons and the result category of each builtin.
namespace clause::semantic::types {
// The fact of an operator's or a builtin call's result; none for another expression. An operation that always raises
// is none().
std::optional<Id> operation_fact(Inference &inference, FunctionRef function, const ast::Expression &expression);
} // namespace clause::semantic::types
