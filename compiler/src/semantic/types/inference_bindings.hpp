#pragma once
#include "inference.hpp"
#include <set>

namespace clause::semantic::types {
// What a waiting pattern matches of the value it waits for: all of it, each element of a list, or each key or value
// of a map.
enum class Part : std::uint8_t { whole, elements, keys, values };

struct Waiting {
    ast::ExprId pattern;
    Part part;
};

struct BindingFacts {
    // Borrow one function's semantic identities; each clause has disjoint local slots.
    FunctionRef function;
    Inference &inference;
    // Index syntax events once rather than scanning the complete binding table for every read.
    std::map<const ast::Expression *, const Binding *> events;
    // Track the facts of successful definitions, parts of matched values included; unproved values remain top.
    std::map<BindingId, Fact> values;
    // Identities defined by several case/if clauses stay top; their clause-specific facts are not joined.
    std::set<BindingId> shared;

    // Index validated bindings within the same batch inference ceiling.
    BindingFacts(FunctionRef function, Inference &inference, std::size_t &work);
    // Missing facts and nonread events are conservative, never argument-index lookup failures.
    Fact read(const ast::ExprId &id) const;
    // Publish the definitions of a pattern with the facts of the values they match, after its successful match.
    void publish(const ast::ExprId &pattern, Fact fact, std::size_t &work);
    // Register the patterns that match an expression's operand once its value is known: case clauses wait for the
    // scrutinee, a try's of clauses for its body's value, generator patterns for their input.
    void expect(const ast::ExprValue &value);
    // Publish the patterns waiting for `expression`, whose value has `fact`.
    void matched(const ast::Expression &expression, Fact fact, std::size_t &work);

    // Register clauses' patterns with the expression whose value they match.
    void expect_clauses(const ast::ExprId &value, const std::vector<ast::BranchClause> &clauses);
    // Register a generator's patterns with its input.
    void expect_generator(const ast::Qualifier &qualifier);

    // The patterns waiting for each expression's value.
    std::map<const ast::Expression *, std::vector<Waiting>> waiting;
};
} // namespace clause::semantic::types
