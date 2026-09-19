#include "children.hpp"

namespace erlang_aot::ast {
void Children::body(const std::vector<ExprId> &values) const {
    if (values.empty())
        throw std::invalid_argument("empty expression body");
    for (const auto &id : values)
        child(id);
}

void Children::guard(const GuardSyntax &value) const {
    source(value.source);
    if (value.alternatives.empty())
        throw std::invalid_argument("empty guard alternatives");
    for (const auto &alternative : value.alternatives) {
        source(alternative.source);
        body(alternative.tests);
    }
}

void Children::pattern(const PatternSyntaxId &id, bool restricted) const {
    const auto &value = builder.view().pattern(id);
    source(value.source);
    if (std::holds_alternative<RestrictedPattern>(value.value) != restricted)
        throw std::invalid_argument("incorrect pattern grammar category");
}

void Children::branch(const BranchClause &value) const {
    source(value.source);
    pattern(value.pattern, false);
    if (value.guard)
        guard(*value.guard);
    body(value.body);
}

void Children::branches(const std::vector<BranchClause> &values) const {
    if (values.empty())
        throw std::invalid_argument("empty branch clauses");
    for (const auto &value : values)
        branch(value);
}

void Children::operator()(const BlockExpression &value) const { body(value.body); }

void Children::operator()(const CaseExpression &value) const {
    child(value.value);
    branches(value.clauses);
}

void Children::operator()(const IfExpression &value) const {
    if (value.clauses.empty())
        throw std::invalid_argument("empty if clauses");
    for (const auto &clause : value.clauses) {
        source(clause.source);
        guard(clause.guard);
        body(clause.body);
    }
}

void Children::operator()(const ReceiveExpression &value) const {
    if (!value.clauses.empty())
        branches(value.clauses);
    if (!value.after && value.clauses.empty())
        throw std::invalid_argument("empty receive");
    if (value.after) {
        source(value.after->source);
        child(value.after->timeout);
        body(value.after->body);
    }
}
} // namespace erlang_aot::ast
