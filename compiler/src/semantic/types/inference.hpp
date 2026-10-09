#pragma once
#include "../calls.hpp"
#include "domain.hpp"
#include <set>

namespace clause::semantic::types {
struct Fact {
    // A top-valued parameter may still carry an exact input/result relation.
    Id type;
    std::optional<std::size_t> argument = {};
    bool operator==(const Fact &) const = default;
};

struct FunctionType {
    // One possible clause (or branch of a case or if ending it): the arguments' facts after its head and guard, and
    // its result.
    std::vector<Id> inputs;
    Fact result;
    // Whether every argument value within `inputs` that reaches the clause enters it (exact patterns, type tests):
    // a call whose arguments are within them can enter no later function type.
    bool exact = false;
    bool operator==(const FunctionType &) const = default;
};

// Function types a function or fun keeps at most; past it the last ones merge into one.
inline constexpr std::size_t FUNCTION_TYPES = 8;

struct Summary {
    // Inputs are term() for exported functions and functions fun F/A names, else their callers' joined arguments.
    std::vector<Id> inputs;
    Fact result;
    // The entry domain: each argument's join over the possible clauses of its fact after the head and guard.
    std::vector<Id> entry = {};
    // The function types of the possible clauses, in source order; none when a budget or widening gave them up.
    std::vector<FunctionType> types = {};
};

struct Inference {
    // Own implementation facts separately from declared contracts and their budgets.
    explicit Inference(Limits limits) : graph(limits) {}

    Graph graph;
    std::map<const Function *, Summary> functions;
    // Borrow stable AST nodes for location-aware inspection without mutating syntax.
    std::map<const ast::Expression *, Fact> expressions;
    // Preserve validated call targets for fresh instantiation at each expression.
    std::map<const ast::Expression *, FunctionRef> callees;
    // The inputs of functions entered only by direct calls of the batch, joined from those calls.
    std::map<const Function *, std::vector<Id>> inputs;
    // The facts fun F/A read in the current pass, by function, to check they were final.
    std::map<const Function *, Id> fun_reads;
    // The members of the recursive component being solved: their domains are not final yet.
    std::set<const Function *> solving;
    // Set for a last pass in which fun F/A has an unknown result, when passes did not settle.
    bool opaque_funs = false;
    // Members of recursive components, whose calls are never re-analysed.
    std::set<const Function *> recursive;
    // How many callee re-analyses for a call are nested now, and the work they spent in the current pass.
    std::size_t reanalyses = 0;
    std::size_t reanalysis_work = 0;
};

// Infer supported bodies iteratively; recursive components iterate from bottom to a fixed point and widen
// to top after a bounded number of rounds. Budget exhaustion conservatively loses precision.
std::unique_ptr<Inference> infer(const CallGraph &calls, Limits limits = {});
} // namespace clause::semantic::types
