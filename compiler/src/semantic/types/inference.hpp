#pragma once
#include "../calls.hpp"
#include "domain.hpp"

namespace clause::semantic::types {
struct Fact {
    // A top-valued parameter may still carry an exact input/result relation.
    Id type;
    std::optional<std::size_t> argument = {};
    bool operator==(const Fact &) const = default;
};

struct Summary {
    // Unknown inputs remain top, including exports and functions with specifications.
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
};

// Infer supported bodies iteratively; recursive components iterate from bottom to a fixed point and widen
// to top after a bounded number of rounds. Budget exhaustion conservatively loses precision.
std::unique_ptr<Inference> infer(const CallGraph &calls, Limits limits = {});
} // namespace clause::semantic::types
