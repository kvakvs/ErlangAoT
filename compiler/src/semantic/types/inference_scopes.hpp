#pragma once
#include "inference_bindings.hpp"

// Scopes of narrowed facts in the body walk (docs/semantic.md#inference): clauses narrow by their patterns and guards,
// the right operand of andalso by its left one, comprehension elements by their filters, maybe bodies by their ?=
// matches; catch, try and every clause restore the facts from before them.
namespace clause::semantic::types {
// What a walk frame does: evaluate an expression's operands or the expression itself, save, restore or reset the
// facts of a scope, assume a test, enter, guard and leave a clause (`head`: a function clause), open and close the
// join of the facts at the end of a construct's completed clauses (`complete`: add the current facts to it;
// `finish`: add them when a try or maybe body completed), or match a maybe's ?= pattern (`bind`).
enum class Step : std::uint8_t {
    visit,
    ready,
    save,
    restore,
    reset,
    assume,
    enter,
    guarded,
    leave,
    enter_head,
    guarded_head,
    leave_head,
    open,
    close,
    complete,
    finish,
    bind
};

struct Frame {
    // The expression evaluated, the test assumed or the construct whose clause `clause` is entered (for `bind`, the
    // position of the ?= match in its maybe's body).
    ast::ExprId expression;
    Step step = Step::visit;
    std::size_t clause = 0;
};

// Push the frames that evaluate an expression: its operands in evaluation order with the scope steps they need, then
// its ready step.
void expand(const Module &module, const ast::ExprId &id, std::vector<Frame> &pending);
// Push the frames of an anonymous fun's clauses, without the fun's own evaluation: each entered, guarded, evaluated
// and left.
void expand_clauses(const Module &module, const ast::ExprId &fun, std::vector<Frame> &pending);
// Push the frames of a function's clauses: each head, guard and body.
void expand_heads(const ast::Function &definition, std::vector<Frame> &pending);
// Run a scope step (anything but visit and ready).
void scope_step(BindingFacts &bindings, const Frame &frame, std::size_t &work);
} // namespace clause::semantic::types
