#include "binding_state.hpp"
#include "capabilities.hpp"
#include "pattern_state.hpp"
#include <cstdint>

namespace erlang_aot::semantic {
namespace {
enum class Action : std::uint8_t {
    expression,
    pattern,
    conditional_enter,
    conditional_exit,
    siblings_enter,
    sibling_end,
    siblings_exit
};

struct Visit {
    // Explicit tasks preserve RHS-first matches and isolate conditionally evaluated definitions.
    ast::ExprId id;
    Action action = Action::expression;
};

struct SiblingScope {
    // All siblings read the same incoming scope; successful definitions accumulate for later expressions.
    BindingEnvironment incoming;
    BindingEnvironment accumulated;
};

// Merge constraints without exposing an earlier sibling's new names to the next sibling's reads.
void finish_sibling(BindingEnvironment &environment, SiblingScope &scope) {
    scope.accumulated.names.insert(environment.names.begin(), environment.names.end());
    scope.accumulated.unsafe.insert(environment.unsafe.begin(), environment.unsafe.end());
    environment = scope.incoming;
    environment.checks.insert(scope.accumulated.names.begin(), scope.accumulated.names.end());
}

// Sequence nodes export immediately; ordinary sibling operands export together after all are analyzed.
void schedule_children(const Module &module, const ast::ExprId &id, const ast::ExprValue &value,
                       std::vector<Visit> &pending) {
    const auto children = std::holds_alternative<ast::RecordExpression>(value)
                              ? expression_children(module, module.syntax->expression(id))
                              : binding_children(value);
    const bool siblings = children.size() > 1 && !std::holds_alternative<ast::BlockExpression>(value);
    if (siblings) {
        pending.push_back({id, Action::siblings_exit});
    }
    for (auto child = children.rbegin(); child != children.rend(); ++child) {
        if (siblings) {
            pending.push_back({*child, Action::sibling_end});
        }
        pending.push_back({*child});
    }
    if (siblings) {
        pending.push_back({id, Action::siblings_enter});
    }
}

// A skipped RHS must not publish its new names, but later reads must diagnose them as unsafe.
void finish_conditional(BindingEnvironment &environment, BindingEnvironment incoming) {
    for (const auto &[name, identity] : environment.names) {
        if (!incoming.names.contains(name)) {
            incoming.unsafe.insert(name);
        }
    }
    incoming.unsafe.insert(environment.unsafe.begin(), environment.unsafe.end());
    environment = std::move(incoming);
}

// Schedule short-circuit operands with a private environment around the optional RHS.
bool conditional(const ast::ExprValue &value, std::vector<Visit> &pending) {
    const auto *binary = std::get_if<ast::BinaryExpression>(&value);
    if (!binary ||
        (binary->operation != ast::BinaryOperator::and_also && binary->operation != ast::BinaryOperator::or_else)) {
        return false;
    }
    pending.push_back({binary->right, Action::conditional_exit});
    pending.push_back({binary->right});
    pending.push_back({binary->right, Action::conditional_enter});
    pending.push_back({binary->left});
    return true;
}

// Guards may read both operands for diagnostics, but a match can never introduce guard bindings.
void match(BindingAnalysis &state, const ast::ExprId &id, const ast::MatchExpression &value,
           const BindingContext context, std::vector<Visit> &pending) {
    if (context == BindingContext::guard) {
        report(state.module, &state.module.syntax->expression(id).source, "guards cannot bind variables", state.out);
        pending.push_back({value.left});
    } else {
        pending.push_back({value.left, Action::pattern});
    }
    pending.push_back({value.right});
}

// Ordinary value traversal never descends into deferred branch, exception or closure scopes.
void expression(BindingAnalysis &state, const ast::ExprId &id, BindingEnvironment &environment,
                const BindingContext context, std::vector<Visit> &pending) {
    const auto &value = state.module.syntax->expression(id).value;
    if (const auto *binary = std::get_if<ast::Bitstring>(&value)) {
        pattern_binary(state, id, *binary, false);
    }
    if (const auto *assignment = std::get_if<ast::MatchExpression>(&value)) {
        match(state, id, *assignment, context, pending);
        return;
    }
    if (conditional(value, pending)) {
        return;
    }
    BindingCandidate scope{environment, {}};
    state.read(id, scope, context);
    if (state.work <= state.limit) {
        schedule_children(state.module, id, value, pending);
    }
}

// Charge copied scope entries as well as nodes so wide nested scopes cannot evade the shared budget.
bool scope_budget(BindingAnalysis &state, const Visit &visit, const BindingEnvironment &environment) {
    const auto amount = environment.names.size() + environment.unsafe.size() + environment.checks.size() + 1;
    return state.spend(visit.id, amount);
}

// Task boundaries are the only publication points; no pattern walk mutates its incoming environment.
bool execute(BindingAnalysis &state, const Visit &visit, BindingEnvironment &environment, const BindingContext context,
             std::vector<Visit> &pending, std::vector<std::unique_ptr<BindingEnvironment>> &conditional_scopes,
             std::vector<std::unique_ptr<SiblingScope>> &sibling_scopes) {
    if (visit.action != Action::expression && !scope_budget(state, visit, environment)) {
        return false;
    }
    switch (visit.action) {
    case Action::pattern: {
        BindingCandidate candidate{environment, {}};
        bind_pattern(state, visit.id, candidate, context);
        candidate.commit(environment);
        break;
    }
    case Action::conditional_enter:
        conditional_scopes.push_back(std::make_unique<BindingEnvironment>(environment));
        break;
    case Action::conditional_exit:
        finish_conditional(environment, std::move(*conditional_scopes.back()));
        conditional_scopes.pop_back();
        break;
    case Action::expression:
        expression(state, visit.id, environment, context, pending);
        break;
    case Action::siblings_enter:
        sibling_scopes.push_back(std::make_unique<SiblingScope>(environment, environment));
        break;
    case Action::sibling_end:
        finish_sibling(environment, *sibling_scopes.back());
        break;
    case Action::siblings_exit:
        environment = std::move(sibling_scopes.back()->accumulated);
        sibling_scopes.pop_back();
        break;
    }
    return true;
}
} // namespace

void bind_expressions(BindingAnalysis &state, const std::vector<ast::ExprId> &roots, BindingEnvironment &environment,
                      const BindingContext context) {
    std::vector<Visit> pending;
    for (auto root = roots.rbegin(); root != roots.rend(); ++root) {
        pending.push_back({*root});
    }
    // Indirection avoids allocating map moves during Windows scope-stack relocation.
    std::vector<std::unique_ptr<BindingEnvironment>> conditional_scopes;
    std::vector<std::unique_ptr<SiblingScope>> sibling_scopes;
    while (!pending.empty()) {
        const auto visit = pending.back();
        pending.pop_back();
        if (!state.spend(visit.id) ||
            !execute(state, visit, environment, context, pending, conditional_scopes, sibling_scopes)) {
            return;
        }
    }
}
} // namespace erlang_aot::semantic
