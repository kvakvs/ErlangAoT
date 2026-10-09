#pragma once
#include "inference.hpp"
#include <optional>

// Facts of the values an expression constructs (docs/semantic.md#inference): literals, tuples, maps and bitstrings,
// built from the facts already recorded for their operands.
namespace clause::semantic::types {
// The fact of a literal or constructed value; none for an expression that constructs no value of its own.
std::optional<Id> constructed_fact(Inference &inference, const ast::Module &syntax, const ast::ExprValue &value);
// Whether a fact holds exactly one value (an integer, an atom, [] or a tuple or map of such facts): only such facts
// are exact map keys.
bool singular(const Graph &graph, Id fact);
} // namespace clause::semantic::types
