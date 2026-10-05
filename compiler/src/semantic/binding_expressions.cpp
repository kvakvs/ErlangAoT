#include "binding_state.hpp"
#include "capabilities.hpp"
#include "pattern_state.hpp"
#include <algorithm>
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
    siblings_exit,
    branch,
    branch_end
};

struct Visit {
    // Explicit tasks preserve RHS-first matches and isolate conditionally evaluated definitions.
    ast::ExprId id;
    Action action = Action::expression;
    // Branch tasks name the case or if clause they start or finish.
    std::size_t clause = 0;
};

struct SiblingScope {
    // All siblings read the same incoming scope; successful definitions accumulate for later expressions.
    BindingEnvironment incoming;
    BindingEnvironment accumulated;
};

struct CaseScope {
    // Every clause starts from the scope after a case scrutinee, before an if or after a try body; finished scopes
    // wait for the join.
    BindingEnvironment incoming;
    std::vector<BindingEnvironment> clauses;
    // A try's catch clauses (from `first_handler` on) start before the try with the body's names unsafe (OTP Uvt).
    std::optional<BindingEnvironment> handlers = {};
    std::size_t first_handler = 0;
};

struct Scopes {
    // Indirection avoids allocating map moves during Windows scope-stack relocation.
    std::vector<std::unique_ptr<BindingEnvironment>> conditional;
    std::vector<std::unique_ptr<SiblingScope>> siblings;
    std::vector<std::unique_ptr<CaseScope>> cases;
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

// A catch may stop its expression anywhere, so names bound inside it become unsafe afterwards (OTP vtunsafe).
bool protect(const ast::ExprValue &value, std::vector<Visit> &pending) {
    const auto *guarded = std::get_if<ast::CatchExpression>(&value);
    if (!guarded) {
        return false;
    }
    pending.push_back({guarded->expression, Action::conditional_exit});
    pending.push_back({guarded->expression});
    pending.push_back({guarded->expression, Action::conditional_enter});
    return true;
}

// Everything a try binds is unsafe afterwards: its body and clauses form one conditional scope, then the after body
// another. Of and catch clauses start once the body is analyzed.
bool attempt(const ast::ExprId &id, const ast::ExprValue &value, std::vector<Visit> &pending) {
    const auto *guarded = std::get_if<ast::TryExpression>(&value);
    if (!guarded) {
        return false;
    }
    if (guarded->after) {
        pending.push_back({id, Action::conditional_exit});
        pending.insert(pending.end(), guarded->after->rbegin(), guarded->after->rend());
        pending.push_back({id, Action::conditional_enter});
    }
    pending.push_back({id, Action::conditional_exit});
    if (!branch_clauses(value).empty()) {
        pending.push_back({id, Action::branch});
    }
    pending.insert(pending.end(), guarded->body.rbegin(), guarded->body.rend());
    pending.push_back({id, Action::conditional_enter});
    return true;
}

// A case evaluates its scrutinee in the enclosing scope before any clause is bound; an if starts with its clauses.
bool branches(const ast::ExprId &id, const ast::ExprValue &value, std::vector<Visit> &pending) {
    if (!std::holds_alternative<ast::CaseExpression>(value) && !std::holds_alternative<ast::IfExpression>(value)) {
        return false;
    }
    pending.push_back({id, Action::branch});
    if (const auto *selection = std::get_if<ast::CaseExpression>(&value)) {
        pending.push_back({selection->value});
    }
    return true;
}

// A catch clause's stack variable must be new: neither bound before nor in its class or reason pattern.
void bind_stack(BindingAnalysis &state, const ast::ExprId &id, BindingCandidate &head) {
    const auto &expression = state.module.syntax->expression(id);
    const auto &name = std::get<ast::Variable>(expression.value).name;
    if (name != U"_" && (head.find(name) || head.incoming.unsafe.contains(name))) {
        report(state.module, &expression.source, "stacktrace variable " + utf8(name) + " must not be previously bound",
               state.out);
        return;
    }
    bind_pattern(state, id, head, BindingContext::body);
}

// Bind a clause pattern; a catch clause matches Class:Reason:Stack left to right.
void bind_head(BindingAnalysis &state, const Branch &clause, BindingCandidate &head) {
    if (clause.handler && clause.handler->exception_class) {
        bind_pattern(state, *clause.handler->exception_class, head, BindingContext::body);
    }
    if (clause.pattern) {
        bind_pattern(state, *clause.pattern, head, BindingContext::body);
    }
    if (clause.handler && clause.handler->stacktrace) {
        bind_stack(state, *clause.handler->stacktrace, head);
    }
}

// The guard of a catch clause must not read its stack variable.
std::optional<std::u32string> stack_name(const ast::Module &syntax, const Branch &clause) {
    if (!clause.handler || !clause.handler->stacktrace) {
        return {};
    }
    const auto &name = std::get<ast::Variable>(syntax.expression(*clause.handler->stacktrace).value).name;
    return name == U"_" ? std::nullopt : std::optional{name};
}

// Bind one clause's pattern and guard over the incoming scope, then schedule its body before the clause end.
void begin_branch(BindingAnalysis &state, const Visit &visit, BindingEnvironment &environment,
                  std::vector<Visit> &pending) {
    const auto clause = branch_clauses(state.module.syntax->expression(visit.id).value).at(visit.clause);
    BindingCandidate head{environment, {}};
    bind_head(state, clause, head);
    if (clause.guard) {
        state.guard_stack = stack_name(*state.module.syntax, clause);
        bind_guard(state, *clause.guard, head);
        state.guard_stack.reset();
    }
    head.commit(environment);
    pending.push_back({visit.id, Action::branch_end, visit.clause});
    for (auto body = clause.body->rbegin(); body != clause.body->rend(); ++body) {
        pending.push_back({*body});
    }
}

// Names bound by every clause are exported; names bound by only some clauses, or unsafe in any, become unsafe.
BindingEnvironment join_branches(BindingAnalysis &state, const ast::ExprId &id, const CaseScope &scope) {
    auto result = scope.incoming;
    std::vector<BindingId> exports;
    for (const auto &[name, identity] : state.branch_names.back()) {
        const bool everywhere = std::ranges::all_of(scope.clauses, [&name](const auto &clause) {
            return clause.names.contains(name) && !clause.unsafe.contains(name);
        });
        if (everywhere) {
            result.names.emplace(name, identity);
            exports.push_back(identity);
        } else {
            result.unsafe.insert(name);
        }
    }
    for (const auto &clause : scope.clauses) {
        result.unsafe.insert(clause.unsafe.begin(), clause.unsafe.end());
    }
    state.function.exports.insert_or_assign(&state.module.syntax->expression(id), std::move(exports));
    return result;
}

// Open the clause scope of a case, if or try; a try's handler scope is the try's conditional scope (innermost).
std::unique_ptr<CaseScope> open_branches(const ast::ExprValue &value, const BindingEnvironment &environment,
                                         const Scopes &scopes) {
    auto scope = std::make_unique<CaseScope>(environment, std::vector<BindingEnvironment>{});
    if (std::holds_alternative<ast::TryExpression>(value)) {
        scope->first_handler = first_handler(value);
        scope->handlers = environment;
        finish_conditional(*scope->handlers, *scopes.conditional.back());
    }
    return scope;
}

// Start a clause from the case's incoming scope (a try's catch clauses from its handler scope); the first clause
// opens the case scope.
void branch(BindingAnalysis &state, const Visit &visit, BindingEnvironment &environment, std::vector<Visit> &pending,
            Scopes &scopes) {
    if (visit.clause == 0) {
        const auto &value = state.module.syntax->expression(visit.id).value;
        scopes.cases.push_back(open_branches(value, environment, scopes));
        state.branch_names.emplace_back();
    }
    const auto &scope = *scopes.cases.back();
    environment = scope.handlers && visit.clause >= scope.first_handler ? *scope.handlers : scope.incoming;
    begin_branch(state, visit, environment, pending);
}

// Record the clause's new names for later clauses, then start the next clause or join them all.
void branch_end(BindingAnalysis &state, const Visit &visit, BindingEnvironment &environment,
                std::vector<Visit> &pending, Scopes &scopes) {
    auto &current = *scopes.cases.back();
    for (const auto &[name, identity] : environment.names) {
        if (!current.incoming.names.contains(name)) {
            state.branch_names.back().emplace(name, identity);
        }
    }
    current.clauses.push_back(std::move(environment));
    if (visit.clause + 1 < branch_clauses(state.module.syntax->expression(visit.id).value).size()) {
        pending.push_back({visit.id, Action::branch, visit.clause + 1});
        return;
    }
    environment = join_branches(state, visit.id, current);
    state.branch_names.pop_back();
    scopes.cases.pop_back();
}

// Guards may read both operands for diagnostics, but a match can never introduce guard bindings.
void match(const BindingAnalysis &state, const ast::ExprId &id, const ast::MatchExpression &value,
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
void expression(BindingAnalysis &state, const ast::ExprId &id, const BindingEnvironment &environment,
                const BindingContext context, std::vector<Visit> &pending) {
    const auto &value = state.module.syntax->expression(id).value;
    if (const auto *binary = std::get_if<ast::Bitstring>(&value)) {
        pattern_binary(state, id, *binary, false);
    }
    if (const auto *assignment = std::get_if<ast::MatchExpression>(&value)) {
        match(state, id, *assignment, context, pending);
        return;
    }
    if (conditional(value, pending) || branches(id, value, pending) || protect(value, pending) ||
        attempt(id, value, pending)) {
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

// Open or close the conditional, sibling and case scopes that surround scheduled expressions.
void scope(BindingAnalysis &state, const Visit &visit, BindingEnvironment &environment, std::vector<Visit> &pending,
           Scopes &scopes) {
    switch (visit.action) {
    case Action::conditional_enter:
        scopes.conditional.push_back(std::make_unique<BindingEnvironment>(environment));
        break;
    case Action::conditional_exit:
        finish_conditional(environment, std::move(*scopes.conditional.back()));
        scopes.conditional.pop_back();
        break;
    case Action::siblings_enter:
        scopes.siblings.push_back(std::make_unique<SiblingScope>(environment, environment));
        break;
    case Action::sibling_end:
        finish_sibling(environment, *scopes.siblings.back());
        break;
    case Action::siblings_exit:
        environment = std::move(scopes.siblings.back()->accumulated);
        scopes.siblings.pop_back();
        break;
    case Action::branch:
        branch(state, visit, environment, pending, scopes);
        break;
    default:
        branch_end(state, visit, environment, pending, scopes);
        break;
    }
}

// Task boundaries are the only publication points; no pattern walk mutates its incoming environment.
bool execute(BindingAnalysis &state, const Visit &visit, BindingEnvironment &environment, const BindingContext context,
             std::vector<Visit> &pending, Scopes &scopes) {
    if (visit.action != Action::expression && !scope_budget(state, visit, environment)) {
        return false;
    }
    if (visit.action == Action::pattern) {
        BindingCandidate candidate{environment, {}};
        bind_pattern(state, visit.id, candidate, context);
        candidate.commit(environment);
    } else if (visit.action == Action::expression) {
        expression(state, visit.id, environment, context, pending);
    } else {
        scope(state, visit, environment, pending, scopes);
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
    Scopes scopes;
    while (!pending.empty()) {
        const auto visit = pending.back();
        pending.pop_back();
        if (!state.spend(visit.id) || !execute(state, visit, environment, context, pending, scopes)) {
            return;
        }
    }
}
} // namespace erlang_aot::semantic
