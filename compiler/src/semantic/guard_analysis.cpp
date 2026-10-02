#include "capabilities.hpp"
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

    bool operator()(const ast::RecordExpression &) const { return true; }

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
void call(BindingAnalysis &state, const ast::ExprId &id, const ast::CallExpression &call, bool guard, bool top) {
    const auto resolved = guard_identity(state, id, call, guard && top);
    const auto &expression = state.module.syntax->expression(id);
    if (!resolved) {
        if (guard) {
            report(state.module, &expression.source, "illegal guard call (not an authorized erlang guard signature)",
                   state.out);
        }
        return;
    }
    const auto &target = state.module.syntax->expression(ungroup(*state.module.syntax, call.target)).value;
    const auto *name = std::get_if<ast::Atom>(&target);
    const bool legacy = guard && top && name && name->name != resolved->name;
    state.function.services.emplace(&expression,
                                    ServiceResolution{*resolved, true, legacy, immediate_service(*resolved)});
}

struct Visit {
    // Parentheses preserve the top-level legacy-test context; other descendants are ordinary guard expressions.
    ast::ExprId id;
    bool top;
};

// Node authorization and child scheduling remain independent so an invalid parent cannot hide operands.
void visit(BindingAnalysis &state, const Visit &visit, bool guard, std::vector<Visit> &pending) {
    const auto &expression = state.module.syntax->expression(visit.id);
    if (const auto *value = std::get_if<ast::CallExpression>(&expression.value)) {
        call(state, visit.id, *value, guard, visit.top);
    } else if (guard && !std::visit(GuardSyntax{}, expression.value)) {
        report(state.module, &expression.source, "illegal guard expression", state.out);
    }
    const auto children = std::holds_alternative<ast::CallExpression>(expression.value)
                              ? std::get<ast::CallExpression>(expression.value).arguments
                              : binding_children(expression.value);
    const bool top = visit.top && std::holds_alternative<ast::Group>(expression.value);
    for (const auto &child : children) {
        pending.push_back({child, top});
    }
}

// Every operand is traversed even behind constant lazy branches; no folding can hide semantic errors.
void expressions(BindingAnalysis &state, const std::vector<ast::ExprId> &roots, bool guard, bool legacy = true) {
    std::vector<Visit> pending;
    pending.reserve(roots.size());
    for (const auto &root : roots) {
        pending.push_back({root, guard && legacy});
    }
    while (!pending.empty()) {
        const auto visit = pending.back();
        pending.pop_back();
        if (!state.spend(visit.id)) {
            return;
        }
        semantic::visit(state, visit, guard, pending);
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

void resolve_services(Module &module, const Reporter &out, std::size_t work_limit) {
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
