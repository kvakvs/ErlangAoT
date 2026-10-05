#pragma once
#include "declarations.hpp"

namespace erlang_aot::semantic {
struct FunctionRef {
    // Borrow stable batch-owned declarations; no syntax or LLVM ownership is transferred.
    Module *module;
    Function *function;
    bool operator==(const FunctionRef &) const = default;
};

struct Call {
    // Associate each direct call expression with its resolved declaration.
    ast::ExprId expression;
    FunctionRef caller;
    FunctionRef callee;
};

struct Component {
    // Functions that reach each other through direct calls; recursive when a member can reach itself.
    std::vector<FunctionRef> members;
    bool recursive = false;
};

struct CallGraph {
    // Retain resolution, strongly connected components with callees first, and their flattened order.
    std::vector<Call> calls;
    std::vector<Component> components;
    std::vector<FunctionRef> order;
};

// Resolve only within one compilation batch; recursive components are ordered, never rejected.
CallGraph resolve_calls(std::span<const std::unique_ptr<Module>> modules, const Reporter &out);
} // namespace erlang_aot::semantic
