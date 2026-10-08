#pragma once
#include "declarations.hpp"

namespace clause::semantic {
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
// The name a module declares with -module, if any.
std::optional<std::u32string> declared_module(const ast::Module &syntax);
// The modules a module's functions name with a literal atom: M:F(...), fun M:F/A and apply(M, F, Args).
std::set<std::u32string> referenced_modules(const ast::Module &syntax);
} // namespace clause::semantic
