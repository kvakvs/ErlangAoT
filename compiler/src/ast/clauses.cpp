#include "builder.hpp"
#include "storage.hpp"

namespace erlang_aot::ast {
// Store a category-safe pattern wrapper only after validating its expression owner.
PatternSyntaxId Builder::pattern(PatternValue value, NodeSource source) {
    validate(source);
    const auto &child = std::visit([](const auto &pattern) -> const ExprId & { return pattern.expression; }, value);
    validate(module_.expression(child).source);
    return module_.storage_->patterns.append({std::move(value), std::move(source)});
}

// Enforce nonempty guard groups and owned expression children without semantic checks.
void Builder::validate(const GuardSyntax &guard) const {
    validate(guard.source);
    if (guard.alternatives.empty())
        throw std::invalid_argument("empty guard alternatives");
    for (const auto &alternative : guard.alternatives) {
        validate(alternative.source);
        if (alternative.tests.empty())
            throw std::invalid_argument("empty guard conjunction");
        for (const auto &test : alternative.tests)
            validate(module_.expression(test).source);
    }
}

// Reject incomplete clauses and permissive candidates in restricted function heads.
void Builder::validate(const FunctionClause &clause) const {
    validate(clause.source);
    for (const auto &id : clause.arguments) {
        const auto &argument = module_.pattern(id);
        validate(argument.source);
        if (!std::holds_alternative<RestrictedPattern>(argument.value)) {
            throw std::invalid_argument("function argument requires restricted pattern syntax");
        }
    }
    if (clause.guard)
        validate(*clause.guard);
    if (clause.body.empty())
        throw std::invalid_argument("empty function body");
    for (const auto &child : clause.body)
        validate(module_.expression(child).source);
}

// Publish functions only when every clause is complete and shares a single arity.
void Builder::validate(const FormValue &value) const {
    const auto *function = std::get_if<Function>(&value);
    if (!function)
        return;
    if (function->clauses.empty())
        throw std::invalid_argument("empty function clauses");
    const auto arity = function->clauses.front().arguments.size();
    for (const auto &clause : function->clauses) {
        if (clause.arguments.size() != arity)
            throw std::invalid_argument("function clause arity mismatch");
        validate(clause);
    }
}
} // namespace erlang_aot::ast
