#include "binding_state.hpp"
#include "records.hpp"
#include <algorithm>
#include <limits>

namespace erlang_aot::semantic {
std::optional<BindingId> BindingCandidate::find(const std::u32string &name) const {
    const auto existing = incoming.names.find(name);
    if (existing != incoming.names.end()) {
        return existing->second;
    }
    const auto added = tentative.find(name);
    return added == tentative.end() ? std::nullopt : std::optional{added->second};
}

void BindingCandidate::commit(BindingEnvironment &destination) const {
    if (valid) {
        destination.names.insert(tentative.begin(), tentative.end());
    }
}

bool BindingAnalysis::spend(const ast::ExprId &id, const std::size_t amount) {
    if (amount > limit - std::min(work, limit)) {
        if (work <= limit) {
            report(module, &module.syntax->expression(id).source, "binding analysis work limit exceeded", out);
        }
        work = limit + 1;
        return false;
    }
    work += amount;
    return true;
}

void BindingAnalysis::read(const ast::ExprId &id, BindingCandidate &scope, const BindingContext context) {
    const auto &expression = module.syntax->expression(id);
    if (!record_budget(*this, id)) {
        return;
    }
    validate_record(module, expression, out);
    const auto *variable = std::get_if<ast::Variable>(&expression.value);
    if (!variable) {
        return;
    }
    const auto identity = scope.find(variable->name);
    if (identity && !scope.incoming.unsafe.contains(variable->name)) {
        function.bindings.push_back({id, *identity, BindingUse::read, context});
        return;
    }
    const auto message = variable->name == U"_"                           ? "wildcard '_' cannot be read"
                         : scope.incoming.unsafe.contains(variable->name) ? "unsafe variable " + utf8(variable->name)
                                                                          : "unbound variable " + utf8(variable->name);
    report(module, &expression.source, message, out);
    scope.valid = false;
}

void BindingAnalysis::define(const ast::ExprId &id, BindingCandidate &scope, const BindingContext context,
                             const std::optional<std::size_t> argument) {
    const auto &expression = module.syntax->expression(id);
    const auto &name = std::get<ast::Variable>(expression.value).name;
    if (name == U"_") {
        return;
    }
    if (scope.incoming.unsafe.contains(name)) {
        read(id, scope, context);
        return;
    }
    if (const auto existing = scope.find(name)) {
        function.bindings.push_back({id, *existing, BindingUse::exact_check, context});
        return;
    }
    if (const auto sibling = scope.incoming.checks.find(name); sibling != scope.incoming.checks.end()) {
        scope.tentative.emplace(name, sibling->second);
        function.bindings.push_back({id, sibling->second, BindingUse::exact_check, context});
        return;
    }
    auto &definitions = function.clause_bindings.at(clause).definitions;
    const BindingId identity{clause, definitions.size()};
    definitions.push_back({name, id, argument});
    scope.tentative.emplace(name, identity);
    function.bindings.push_back({id, identity, BindingUse::definition, context});
}

namespace {
// Each guard sees the completed tentative head, but no alternative can assign a name.
void guards(BindingAnalysis &state, const ast::GuardSyntax &guard, BindingCandidate &head) {
    BindingEnvironment visible = head.incoming;
    head.commit(visible);
    for (const auto &alternative : guard.alternatives) {
        bind_expressions(state, alternative.tests, visible, BindingContext::guard);
    }
}

// Only the successful head candidate is made available to the body; the incoming scope stays empty.
void bind_clause(BindingAnalysis &state, const ast::FunctionClause &clause) {
    const BindingEnvironment incoming;
    BindingCandidate head{incoming, {}};
    for (std::size_t i = 0; i < clause.arguments.size(); ++i) {
        bind_pattern(state, clause.arguments[i], head, BindingContext::head, i);
    }
    if (clause.guard) {
        guards(state, *clause.guard, head);
    }
    BindingEnvironment body;
    head.commit(body);
    bind_expressions(state, clause.body, body, BindingContext::body);
}

// A new analysis or budget failure must not expose stale or partially constructed side tables.
void clear_bindings(Module &module) {
    for (auto &function : module.functions) {
        function.bindings.clear();
        function.clause_bindings.clear();
        function.patterns.clear();
    }
}
} // namespace

void bind_parameters(Module &module, const Reporter &out, const std::size_t work_limit) {
    std::size_t work = 0;
    const auto limit = std::min(work_limit, std::numeric_limits<std::size_t>::max() - 1);
    clear_bindings(module);
    bool failed = false;
    const Reporter transactional = [&](const Diagnostic &diagnostic) {
        failed = failed || diagnostic.severity == Severity::error;
        out(diagnostic);
    };
    for (auto &function : module.functions) {
        const auto &clauses = std::get<ast::Function>(module.syntax->form(function.form).value).clauses;
        function.clause_bindings.resize(clauses.size());
        for (std::size_t i = 0; i < clauses.size(); ++i) {
            BindingAnalysis state{module, function, transactional, i, work, limit};
            bind_clause(state, clauses[i]);
            if (work > limit) {
                clear_bindings(module);
                return;
            }
        }
    }
    if (failed) {
        clear_bindings(module);
    }
}

std::optional<std::size_t> binding_argument(const Function &function, const ast::ExprId &expression) {
    const auto found = std::ranges::find(function.bindings, expression, &Binding::expression);
    if (found == function.bindings.end() || found->use != BindingUse::read) {
        return {};
    }
    const auto identity = found->identity;
    return function.clause_bindings.at(identity.clause).definitions.at(identity.local).argument;
}
} // namespace erlang_aot::semantic
