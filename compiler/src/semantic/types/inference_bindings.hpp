#pragma once
#include "inference.hpp"
#include <set>

namespace erlang_aot::semantic::types {
struct BindingFacts {
    // Borrow one function's semantic identities; each clause has disjoint local slots.
    FunctionRef function;
    Inference &inference;
    // Index syntax events once rather than scanning the complete binding table for every read.
    std::map<const ast::Expression *, const Binding *> events;
    // Track successful whole-value assignments; extracted and unproved values remain top.
    std::map<BindingId, Fact> values;
    // Identities defined by several case/if clauses stay top; their clause-specific facts are not joined.
    std::set<BindingId> shared;

    // Index validated bindings within the same batch inference ceiling.
    BindingFacts(FunctionRef function, Inference &inference, std::size_t &work);
    // Missing facts and nonread events are conservative, never argument-index lookup failures.
    Fact read(const ast::ExprId &id) const;
    // Publish only whole-value definitions after a body match's successful continuation.
    void publish(const ast::ExprId &pattern, Fact fact, std::size_t &work);
};
} // namespace erlang_aot::semantic::types
