#include "expression_capability.hpp"
#include "funs.hpp"
#include "records.hpp"
#include "services.hpp"
#include <algorithm>

namespace erlang_aot::semantic {
std::string_view ExpressionCapability::operator()(const ast::RecordExpression &value) const {
    const auto *layout = record_layout(module, value.identity);
    const bool native = anonymous_record(value.identity) || external_record(module, value.identity);
    return layout || native ? "" : "heap expressions";
}

std::string_view ExpressionCapability::operator()(const ast::RecordAccess &value) const {
    const auto *layout = record_layout(module, value.identity);
    const bool native = anonymous_record(value.identity) || external_record(module, value.identity);
    return layout || native ? "" : "heap expressions";
}

std::string_view ExpressionCapability::operator()(const ast::RecordIndex &value) const {
    const auto *layout = record_layout(module, value.record, syntax.expression(id).source);
    return layout ? "" : "heap expressions";
}

// try ... of ... catch Class:Reason:Stack ... after runs.
std::string_view ExpressionCapability::operator()(const ast::TryExpression &) const { return {}; }

std::string_view ExpressionCapability::operator()(const ast::IntegerLiteral &) const { return {}; }

std::string_view ExpressionCapability::operator()(const ast::CharacterLiteral &) const {
    return integer_literal(syntax, id, word_bits) ? "" : "bignum expressions";
}

std::string_view ExpressionCapability::operator()(const ast::UnaryExpression &value) const {
    return immediate_unary(value.operation) ? "" : "arithmetic";
}

// A fun of an erlang builtin needs builtins callable as values; fun M:F/A with variables needs dynamic lookup.
std::string_view ExpressionCapability::operator()(const ast::LocalFunReference &value) const {
    const auto count = arity(value.arity);
    const FunctionKey key{value.name.name, count.value_or(0)};
    return count && !module.lookup.contains(key) && guard_signature(key) ? "dynamic calls" : "";
}

// fun M:F/A with variables is built at run time; a literal fun erlang:F/A needs builtins callable as values.
std::string_view ExpressionCapability::operator()(const ast::RemoteFunReference &value) const {
    if (dynamic_fun(value)) {
        const auto *count = std::get_if<Integer>(&value.arity);
        const auto valid = count ? arity(*count) : std::optional<std::size_t>{0};
        return valid && *valid <= 255 ? "" : "dynamic calls";
    }
    const auto names = external_fun(value);
    return names && std::get<0>(*names) != U"erlang" ? "" : "dynamic calls";
}

std::string_view ExpressionCapability::operator()(const ast::BinaryExpression &value) const {
    if (immediate_operator(value.operation)) {
        return {};
    }
    if (value.operation == ast::BinaryOperator::and_also || value.operation == ast::BinaryOperator::or_else) {
        return {};
    }
    return value.operation == ast::BinaryOperator::send ? "send expressions" : "arithmetic";
}
} // namespace erlang_aot::semantic
