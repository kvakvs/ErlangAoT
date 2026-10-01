#pragma once
#include "bindings.hpp"

namespace erlang_aot::semantic {
struct BindingAnalysis {
    // Borrow analysis owners and keep a single budget across every clause in the module.
    const Module &module;
    Function &function;
    const Reporter &out;
    std::size_t clause;
    std::size_t &work;
    std::size_t limit;

    // Stop bounded iterative walks at the original node that exhausted the budget.
    bool spend(const ast::ExprId &id, std::size_t amount = 1);
    // Resolve a read or diagnose wildcard, unsafe and unbound names at their own source anchors.
    void read(const ast::ExprId &id, BindingCandidate &scope, BindingContext context);
    // Add a fresh identity or record an exact check without assigning over an existing name.
    void define(const ast::ExprId &id, BindingCandidate &scope, BindingContext context,
                std::optional<std::size_t> argument);
};

// Visit binding-bearing pattern syntax; pattern legality and match plans belong to later stages.
void bind_pattern(BindingAnalysis &state, const ast::ExprId &id, BindingCandidate &scope, BindingContext context,
                  std::optional<std::size_t> argument = {});
// Evaluate RHS bindings before LHS patterns, and preserve sequential expression visibility.
void bind_expressions(BindingAnalysis &state, const std::vector<ast::ExprId> &roots, BindingEnvironment &environment,
                      BindingContext context);
// Enumerate ordinary value children; scope-introducing syntax is handled separately or deferred.
std::vector<ast::ExprId> binding_children(const ast::ExprValue &value);
} // namespace erlang_aot::semantic
