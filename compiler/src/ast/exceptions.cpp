#include "children.hpp"

namespace erlang_aot::ast {
void Children::function_clause(const FunctionClause &value) const {
    source(value.source);
    for (const auto &argument : value.arguments) {
        pattern(argument, true);
    }
    if (value.guard) {
        guard(*value.guard);
    }
    body(value.body);
}

void Children::function_clauses(const std::vector<FunctionClause> &values) const {
    if (values.empty()) {
        throw std::invalid_argument("empty function clauses");
    }
    const auto arity = values.front().arguments.size();
    for (const auto &value : values) {
        if (value.arguments.size() != arity) {
            throw std::invalid_argument("function clause arity mismatch");
        }
        function_clause(value);
    }
}

void Children::operator()(const FunExpression &value) const { function_clauses(value.clauses); }

void Children::handler(const CatchClause &value) const {
    source(value.source);
    pattern(value.reason, true);
    if (value.stacktrace && !value.exception_class) {
        throw std::invalid_argument("stacktrace requires an explicit exception class");
    }
    if (value.guard) {
        guard(*value.guard);
    }
    body(value.body);
}

void Children::operator()(const TryExpression &value) const {
    body(value.body);
    if (value.of) {
        branches(*value.of);
    }
    if (!value.handlers && !value.after) {
        throw std::invalid_argument("try requires catch or after");
    }
    if (value.handlers) {
        if (value.handlers->empty()) {
            throw std::invalid_argument("empty catch clauses");
        }
        for (const auto &clause : *value.handlers) {
            handler(clause);
        }
    }
    if (value.after) {
        body(*value.after);
    }
}

void Children::maybe_item(const ExprId &value) const { child(value); }

void Children::maybe_item(const MaybeMatch &value) const {
    source(value.source);
    pattern(value.pattern, false);
    child(value.value);
}

void Children::operator()(const MaybeExpression &value) const {
    if (value.body.empty()) {
        throw std::invalid_argument("empty maybe body");
    }
    for (const auto &item : value.body) {
        std::visit([this](const auto &part) { maybe_item(part); }, item);
    }
    if (value.otherwise) {
        branches(*value.otherwise);
    }
}
} // namespace erlang_aot::ast
