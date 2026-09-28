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

struct CallGraph {
    // Retain resolution and a callee-before-caller order for subsequent inference.
    std::vector<Call> calls;
    std::vector<FunctionRef> order;
};

// Resolve only within one compilation batch and reject executable dependency cycles.
CallGraph resolve_calls(std::span<const std::unique_ptr<Module>> modules, const Reporter &out);
} // namespace erlang_aot::semantic
