#pragma once
#include "../calls.hpp"
#include "domain.hpp"

namespace erlang_aot::semantic::types {
struct Fact {
    // A top-valued parameter may still carry an exact input/result relation.
    Id type;
    std::optional<std::size_t> argument = {};
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
};

// Infer supported bodies iteratively; budget exhaustion conservatively loses precision.
std::unique_ptr<Inference> infer(const CallGraph &calls, Limits limits = {});
} // namespace erlang_aot::semantic::types
