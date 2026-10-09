#include "function_types.hpp"
#include <algorithm>

namespace clause::semantic::types {
namespace {
// Fold `type` into `into`: their inputs join position by position, their results join.
void absorb(Graph &graph, FunctionType &into, const FunctionType &type) {
    Lattice lattice(graph);
    for (std::size_t index = 0; index < into.inputs.size() && index < type.inputs.size(); ++index) {
        into.inputs[index] = lattice.join(into.inputs[index], type.inputs[index]);
    }
    into.result = merged(graph, into.result, type.result);
    into.exact = false;
}

// Whether two lists of function types have the same inputs, type by type.
bool same_inputs(const std::vector<FunctionType> &left, const std::vector<FunctionType> &right) {
    return std::ranges::equal(left, right,
                              [](const FunctionType &a, const FunctionType &b) { return a.inputs == b.inputs; });
}

// Whether a call with these argument facts can enter a function type: each fact meets its input.
bool admits(Lattice &lattice, const FunctionType &type, const std::vector<Fact> &arguments) {
    for (std::size_t index = 0; index < type.inputs.size() && index < arguments.size(); ++index) {
        if (lattice.meet(arguments[index].type, type.inputs[index]) == lattice.graph().bottom()) {
            return false;
        }
    }
    return type.inputs.size() == arguments.size();
}

// Whether a call with these argument facts surely enters an exact function type, so no later one.
bool settles(Lattice &lattice, const FunctionType &type, const std::vector<Fact> &arguments) {
    if (!type.exact) {
        return false;
    }
    for (std::size_t index = 0; index < type.inputs.size(); ++index) {
        if (!lattice.within(arguments[index].type, type.inputs[index])) {
            return false;
        }
    }
    return true;
}

// Join a selected type's inputs into the inputs selected so far (none at first).
void join_inputs(Lattice &lattice, std::vector<Id> &selected, const std::vector<Id> &inputs) {
    if (selected.empty()) {
        selected = inputs;
        return;
    }
    for (std::size_t index = 0; index < inputs.size() && index < selected.size(); ++index) {
        selected[index] = lattice.join(selected[index], inputs[index]);
    }
}

// A function type's result for a call: a result equal to an argument is that argument's fact, within the result.
Fact instance(Lattice &lattice, const FunctionType &type, const std::vector<Fact> &arguments) {
    const auto argument = type.result.argument;
    if (!argument || *argument >= arguments.size() || type.result.type == lattice.graph().bottom()) {
        return type.result;
    }
    const auto &actual = arguments[*argument];
    return {lattice.meet(actual.type, type.result.type), actual.argument};
}
} // namespace

Fact merged(Graph &graph, const Fact previous, const Fact next, const bool widening) {
    // A value that never exists relates to no argument.
    if (previous.type == graph.bottom()) {
        return next.type == graph.bottom() ? Fact{graph.bottom()} : next;
    }
    if (next.type == graph.bottom()) {
        return previous;
    }
    Lattice lattice(graph);
    const auto type = widening ? lattice.widen(previous.type, next.type) : lattice.join(previous.type, next.type);
    return {type, previous.argument == next.argument ? previous.argument : std::nullopt};
}

std::vector<FunctionType> merge_types(Graph &graph, std::vector<FunctionType> types) {
    std::vector<FunctionType> result;
    for (auto &type : types) {
        const auto same = std::ranges::find(result, type.inputs, &FunctionType::inputs);
        if (same == result.end()) {
            result.push_back(std::move(type));
        } else {
            same->result = merged(graph, same->result, type.result);
        }
    }
    while (result.size() > FUNCTION_TYPES) {
        absorb(graph, result[result.size() - 2], result.back());
        result.pop_back();
    }
    return result;
}

std::vector<FunctionType> next_round(Graph &graph, const std::vector<FunctionType> &previous,
                                     std::vector<FunctionType> next, const bool widening) {
    if (!same_inputs(previous, next)) {
        return next;
    }
    for (std::size_t index = 0; index < next.size(); ++index) {
        next[index].result = merged(graph, previous[index].result, next[index].result, widening);
    }
    return next;
}

Id fun_fact(Lattice &lattice, const std::vector<FunctionType> &types) {
    std::vector<Id> funs;
    funs.reserve(types.size());
    for (const auto &type : types) {
        const auto argument = type.result.argument;
        const bool related = argument && *argument < type.inputs.size() && type.result.type == lattice.graph().top();
        const auto fun = lattice.fun(type.inputs, related ? type.inputs[*argument] : type.result.type, type.exact);
        if (!std::ranges::contains(funs, fun)) {
            funs.push_back(fun);
        }
    }
    return lattice.overloaded(std::move(funs));
}

Selection select(Graph &graph, const std::vector<FunctionType> &types, const std::vector<Fact> &arguments) {
    Lattice lattice(graph);
    Selection selection{{graph.bottom()}};
    for (const auto &type : types) {
        if (!admits(lattice, type, arguments)) {
            continue;
        }
        selection.result = merged(graph, selection.result, instance(lattice, type, arguments));
        join_inputs(lattice, selection.inputs, type.inputs);
        if (settles(lattice, type, arguments)) {
            break;
        }
    }
    return selection;
}
} // namespace clause::semantic::types
