#pragma once
#include "preprocessor/value.hpp"
#include <erlang_aot/compiler/ast/module.hpp>

namespace erlang_aot {
// Parentheses do not change OTP's attribute builder shapes.
inline const ast::Expression &ungroup(const ast::Module &module, ast::ExprId id) {
    while (const auto *group = std::get_if<ast::Group>(&module.expression(id).value)) {
        id = group->expression;
    }
    return module.expression(id);
}

template <typename T> const T &attribute_as(const ast::Module &module, const ast::ExprId &id) {
    const auto *value = std::get_if<T>(&ungroup(module, id).value);
    if (!value) {
        throw EvaluationFailure();
    }
    return *value;
}

// Flatten explicit list tails while rejecting improper or non-list envelopes.
std::vector<ast::ExprId> attribute_list(const ast::Module &module, const ast::ExprId &id);
std::vector<ast::NameArity> attribute_arities(const ast::Module &module, const ast::ExprId &id);
} // namespace erlang_aot
