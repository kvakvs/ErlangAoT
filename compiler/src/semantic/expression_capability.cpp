#include "expression_capability.hpp"
#include "records.hpp"
#include "services.hpp"
#include <algorithm>

namespace erlang_aot::semantic {
std::string_view ExpressionCapability::operator()(const ast::RecordExpression &value) const {
    const auto *layout = record_layout(module, value.identity);
    return !value.base && layout && !layout->native ? "" : "heap expressions";
}

std::string_view ExpressionCapability::operator()(const ast::RecordAccess &value) const {
    const auto *layout = record_layout(module, value.identity);
    return layout && !layout->native ? "" : "heap expressions";
}

std::string_view ExpressionCapability::operator()(const ast::RecordIndex &value) const {
    const auto *layout = record_layout(module, value.record, syntax.expression(id).source);
    return layout && !layout->native ? "" : "heap expressions";
}

// List generators and filters run; binary and map generators arrive with binary and map comprehensions.
std::string_view ExpressionCapability::operator()(const ast::ListComprehension &value) const {
    for (const auto &qualifier : value.qualifiers) {
        for (const auto &part : zipped(qualifier)) {
            if (std::holds_alternative<ast::BinaryGenerator>(part.value) ||
                std::holds_alternative<ast::MapGenerator>(part.value)) {
                return "heap expressions";
            }
        }
    }
    return {};
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

std::string_view ExpressionCapability::operator()(const ast::CallExpression &value) const {
    const auto &target = syntax.expression(ungroup(syntax, value.target)).value;
    if (std::holds_alternative<ast::Atom>(target)) {
        return {};
    }
    const auto *remote = std::get_if<ast::RemoteExpression>(&target);
    if (!remote) {
        return "dynamic calls";
    }
    const bool literal_module =
        std::holds_alternative<ast::Atom>(syntax.expression(ungroup(syntax, remote->module)).value);
    const bool literal_function =
        std::holds_alternative<ast::Atom>(syntax.expression(ungroup(syntax, remote->function)).value);
    return literal_module && literal_function ? "" : "dynamic calls";
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
