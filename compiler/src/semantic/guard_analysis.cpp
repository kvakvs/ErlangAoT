#include "capabilities.hpp"
#include "records.hpp"
#include "services.hpp"
#include <algorithm>
#include <limits>

namespace erlang_aot::semantic {
namespace {
struct GuardSyntax {
    // Closed syntax allowlist checks legality independently of executable support.
    template <typename T> bool operator()(const T &) const { return false; }

    bool operator()(const ast::Variable &) const { return true; }

    bool operator()(const ast::IntegerLiteral &) const { return true; }

    bool operator()(const ast::CharacterLiteral &) const { return true; }

    bool operator()(const ast::Atom &) const { return true; }

    bool operator()(const ast::FloatLiteral &) const { return true; }

    bool operator()(const ast::StringLiteral &) const { return true; }

    bool operator()(const ast::Group &) const { return true; }

    bool operator()(const ast::Tuple &) const { return true; }

    bool operator()(const ast::List &) const { return true; }

    bool operator()(const ast::Bitstring &) const { return true; }

    bool operator()(const ast::RecordExpression &value) const { return !value.base; }

    bool operator()(const ast::RecordAccess &) const { return true; }

    bool operator()(const ast::RecordIndex &) const { return true; }

    bool operator()(const ast::MapExpression &value) const {
        return value.base || std::ranges::none_of(value.fields, [](const auto &field) {
                   return field.kind == ast::MapFieldKind::exact;
               });
    }

    bool operator()(const ast::UnaryExpression &) const { return true; }

    bool operator()(const ast::BinaryExpression &value) const {
        return value.operation != ast::BinaryOperator::send && value.operation != ast::BinaryOperator::append &&
               value.operation != ast::BinaryOperator::subtract_list;
    }
};

// Authorize erlang identities before any lowering or runtime lookup; local body calls retain normal resolution.
void call(BindingAnalysis &state, const ast::ExprId &id, const ast::CallExpression &call, const bool guard,
          const bool top) {
    const auto resolved = guard_identity(state, id, call, guard && top);
    const auto &expression = state.module.syntax->expression(id);
    if (!resolved) {
        if (guard) {
            report(state.module, &expression.source, "illegal guard call (not an authorized erlang guard signature)",
                   state.out);
        } else if (const auto builtin = body_builtin(*state.module.syntax, call)) {
            state.function.services.emplace(&expression,
                                            ServiceResolution{*builtin, false, false, immediate_service(*builtin)});
        }
        return;
    }
    const auto &target = state.module.syntax->expression(ungroup(*state.module.syntax, call.target)).value;
    const auto *name = std::get_if<ast::Atom>(&target);
    const bool legacy = guard && top && name && name->name != resolved->name;
    if (resolved->name == U"is_record" && resolved->arity >= 2) {
        validate_record_test(state.module, expression, call, guard, state.out);
    }
    state.function.services.emplace(&expression,
                                    ServiceResolution{*resolved, true, legacy, immediate_service(*resolved)});
}

struct Visit {
    // Parentheses preserve the top-level legacy-test context; other descendants are ordinary guard expressions.
    ast::ExprId id;
    bool top;
    // Case clause guards nested in a body are guard contexts of their own.
    bool guard;
};

// Each guard test is a top-level legacy-test position.
void push_guard(const ast::GuardSyntax &guard, std::vector<Visit> &pending) {
    for (const auto &alternative : guard.alternatives) {
        for (const auto &test : alternative.tests) {
            pending.push_back({test, true, true});
        }
    }
}

// Schedule a case scrutinee and case/if bodies in the current context and clause guards as top-level guard tests.
bool branch_guards(const ast::Expression &expression, const Visit &visit, std::vector<Visit> &pending) {
    const auto clauses = branch_clauses(expression.value);
    if (clauses.empty()) {
        return false;
    }
    if (const auto *selection = std::get_if<ast::CaseExpression>(&expression.value)) {
        pending.push_back({selection->value, false, visit.guard});
    }
    for (const auto &clause : clauses) {
        if (clause.guard) {
            push_guard(*clause.guard, pending);
        }
        for (const auto &body : *clause.body) {
            pending.push_back({body, false, visit.guard});
        }
    }
    return true;
}

// Node authorization and child scheduling remain independent so an invalid parent cannot hide operands.
void visit(BindingAnalysis &state, const Visit &visit, std::vector<Visit> &pending) {
    const auto &expression = state.module.syntax->expression(visit.id);
    if (const auto *value = std::get_if<ast::CallExpression>(&expression.value)) {
        call(state, visit.id, *value, visit.guard, visit.top);
    } else if (visit.guard && !std::visit(GuardSyntax{}, expression.value)) {
        report(state.module, &expression.source, "illegal guard expression", state.out);
    }
    if (branch_guards(expression, visit, pending)) {
        return;
    }
    const auto children = expression_children(state.module, expression);
    const bool top = visit.top && std::holds_alternative<ast::Group>(expression.value);
    for (const auto &child : children) {
        pending.push_back({child, top, visit.guard});
    }
}

// Every operand is traversed even behind constant lazy branches; no folding can hide semantic errors.
void expressions(BindingAnalysis &state, const std::vector<ast::ExprId> &roots, const bool guard,
                 const bool legacy = true) {
    std::vector<Visit> pending;
    pending.reserve(roots.size());
    for (const auto &root : roots) {
        pending.push_back({root, guard && legacy, guard});
    }
    while (!pending.empty()) {
        const auto visit = pending.back();
        pending.pop_back();
        if (!state.spend(visit.id)) {
            return;
        }
        semantic::visit(state, visit, pending);
    }
}

// Each clause uses a common semantic budget while keeping legacy-test roots separate.
void clause(BindingAnalysis &state, const ast::FunctionClause &clause) {
    if (clause.guard) {
        for (const auto &alternative : clause.guard->alternatives) {
            expressions(state, alternative.tests, true);
        }
    }
    expressions(state, clause.body, false);
}
} // namespace

void resolve_services(Module &module, const Reporter &out, const std::size_t work_limit) {
    std::size_t work = 0;
    const auto limit = std::min(work_limit, std::numeric_limits<std::size_t>::max() - 1);
    bool failed = false;
    const Reporter transactional = [&](const Diagnostic &d) {
        failed = failed || d.severity == Severity::error;
        out(d);
    };
    for (auto &function : module.functions) {
        function.services.clear();
        BindingAnalysis pattern_state{module, function, transactional, 0, work, limit};
        expressions(pattern_state, pattern_reads(module, function), true, false);
        const auto &clauses = std::get<ast::Function>(module.syntax->form(function.form).value).clauses;
        for (std::size_t i = 0; i < clauses.size(); ++i) {
            BindingAnalysis state{module, function, transactional, i, work, limit};
            clause(state, clauses[i]);
        }
    }
    if (failed) {
        for (auto &function : module.functions) {
            function.services.clear();
        }
    }
}
} // namespace erlang_aot::semantic
