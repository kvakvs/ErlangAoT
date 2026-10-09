#pragma once
#include "inference.hpp"
#include "lattice.hpp"

// Function types (docs/semantic.md#inference): the inputs and result of each possible clause, which functions and
// funs keep beside their union summary.
namespace clause::semantic::types {
// The fact where two paths meet: none() adds nothing, only a relation both keep survives, and the types join (or
// widen between rounds of a recursive component).
Fact merged(Graph &graph, Fact previous, Fact next, bool widening = false);
// Merge function types of equal inputs into the first of them (their results join), and keep at most FUNCTION_TYPES:
// the last ones merge into one, their inputs and results joined.
std::vector<FunctionType> merge_types(Graph &graph, std::vector<FunctionType> types);
// A recursive component's function types after a round: each result joined (or widened) with the previous round's
// when the inputs are the same, else the new types.
std::vector<FunctionType> next_round(Graph &graph, const std::vector<FunctionType> &previous,
                                     std::vector<FunctionType> next, bool widening);
// The fun fact of function types (at least one); a result equal to an argument is that argument's input.
Id fun_fact(Lattice &lattice, const std::vector<FunctionType> &types);

// A result for a call: a result equal to an argument is that argument's fact, within the result's type.
Fact instantiated(Lattice &lattice, Fact result, const std::vector<Fact> &arguments);

// The function types a call enters: their joined results, each relation to an argument instantiated with that
// argument's fact, and their joined inputs; none() and no inputs when it enters none.
struct Selection {
    Fact result;
    std::vector<Id> inputs = {};
};

// A call's selection of function types: in order, each type whose inputs every argument fact meets, up to an exact
// one whose inputs hold the arguments (docs/semantic.md#inference).
Selection select(Graph &graph, const std::vector<FunctionType> &types, const std::vector<Fact> &arguments);
} // namespace clause::semantic::types
