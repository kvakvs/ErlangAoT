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

    // Remember an anonymous fun bound whole to a variable, so calls of the variable can evaluate it, and a variable
    // bound to another one's value.
    void bind_lambda(const ast::MatchExpression &match);
    // The anonymous fun bound to the variable a read names, if any.
    const ast::Expression *lambda(const ast::ExprId &read) const;

    // The anonymous funs bound to variables, and how many of them are being evaluated for a call.
    std::map<BindingId, const ast::Expression *> lambdas;
    std::size_t depth = 0;
    // Narrow the variable a read names, and the names bound to the same value, to their meet with `fact`.
    void narrow(const ast::ExprId &read, Id fact);
    // Narrow a name and the names bound to the same value to their meet with `fact`, in `facts` (the current values or
    // a copy of them); the name's new fact.
    Id narrow_identity(std::map<BindingId, Fact> &facts, BindingId identity, Id fact) const;
    // Record that two names are bound to the same value.
    void link(BindingId first, BindingId second);

    // Names bound to the same value as each name.
    std::map<BindingId, std::set<BindingId>> aliases;
    // For each case, if and receive being walked, the join of the facts at the end of each clause that completed.
    std::vector<std::optional<std::map<BindingId, Fact>>> merged;
    // Each argument's join of its facts at the normal return of every clause: the success domain.
    std::vector<Id> success;

    // Narrowing scopes: the facts saved when a clause, andalso operand, catch, try or comprehension began.
    std::vector<std::map<BindingId, Fact>> saved;
    // Clause bodies that can never run: their patterns or guard contradict the facts.
    std::set<const std::vector<ast::ExprId> *> impossible;
    // A function's inputs, each argument's fact at the current clause's entry, and the entry domain.
    std::vector<Id> inputs;
    std::vector<Id> entry;
    std::vector<Id> domain;
    // The patterns waiting for each expression's value.
    std::map<const ast::Expression *, std::vector<Waiting>> waiting;
};
} // namespace clause::semantic::types
