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
    // Pattern errors invalidate tentative bindings even before the module transaction is discarded.
    bool invalid_pattern = false;
    // Names defined by earlier clauses of each enclosing case or if; later clauses reuse them so exports share
    // one identity.
    std::vector<std::map<std::u32string, BindingId>> branch_names = {};
    // The catch clause's stack variable while its guard is analyzed; reading it there is an error.
    std::optional<std::u32string> guard_stack = {};

    // Stop bounded iterative walks at the original node that exhausted the budget.
    bool spend(const ast::ExprId &id, std::size_t amount = 1);
    // Resolve a read or diagnose wildcard, unsafe and unbound names at their own source anchors.
    void read(const ast::ExprId &id, BindingCandidate &scope, BindingContext context);
    // Add a fresh identity or record an exact check without assigning over an existing name.
    void define(const ast::ExprId &id, BindingCandidate &scope, BindingContext context,
                std::optional<std::size_t> argument);
    // Define a name of a fresh candidate: new unless the same candidate already defined it.
    void define_fresh(const ast::ExprId &id, BindingCandidate &scope, BindingContext context,
                      const std::u32string &name);
};

// Validate/normalize a restricted or permissive pattern using the same bounded semantic rules.
void bind_pattern(BindingAnalysis &state, const ast::PatternSyntaxId &id, BindingCandidate &scope,
                  BindingContext context, std::optional<std::size_t> argument = {});
// Body expression '=' supplies its left expression directly; pattern '=' is a compound constraint.
void bind_pattern(BindingAnalysis &state, const ast::ExprId &id, BindingCandidate &scope, BindingContext context,
                  std::optional<std::size_t> argument = {});
// Each guard alternative sees the completed tentative head, but no alternative can assign a name.
void bind_guard(BindingAnalysis &state, const ast::GuardSyntax &guard, const BindingCandidate &head);
// Evaluate RHS bindings before LHS patterns, and preserve sequential expression visibility.
void bind_expressions(BindingAnalysis &state, const std::vector<ast::ExprId> &roots, BindingEnvironment &environment,
                      BindingContext context);
// Enumerate ordinary value children; scope-introducing syntax is handled separately or deferred.
std::vector<ast::ExprId> binding_children(const ast::ExprValue &value);
} // namespace erlang_aot::semantic
