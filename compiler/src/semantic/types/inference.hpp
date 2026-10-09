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

struct Summary {
    // Inputs are term() for exported functions and functions fun F/A names, else their callers' joined arguments.
    std::vector<Id> inputs;
    Fact result;
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
    // The results fun F/A read in the current pass, by function, to check they were final.
    std::map<const Function *, Id> fun_reads;
    // The members of the recursive component being solved: their domains are not final yet.
    std::set<const Function *> solving;
    // Set for a last pass in which fun F/A has an unknown result, when passes did not settle.
    bool opaque_funs = false;
};

// Infer supported bodies iteratively; recursive components iterate from bottom to a fixed point and widen
// to top after a bounded number of rounds. Budget exhaustion conservatively loses precision.
std::unique_ptr<Inference> infer(const CallGraph &calls, Limits limits = {});
} // namespace clause::semantic::types
