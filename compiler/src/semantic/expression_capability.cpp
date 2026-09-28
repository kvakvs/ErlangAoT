#include "expression_capability.hpp"

namespace erlang_aot::semantic {
std::string_view ExpressionCapability::operator()(const ast::IntegerLiteral &) const {
    return integer_literal(syntax, id, word_bits) ? "" : "bignum expressions";
}

std::string_view ExpressionCapability::operator()(const ast::CharacterLiteral &) const {
    return integer_literal(syntax, id, word_bits) ? "" : "bignum expressions";
}

std::string_view ExpressionCapability::operator()(const ast::UnaryExpression &value) const {
    const auto &operand = syntax.expression(ungroup(syntax, value.operand)).value;
    if (value.operation != ast::UnaryOperator::negative || !std::holds_alternative<ast::IntegerLiteral>(operand)) {
        return "arithmetic";
    }
    return integer_literal(syntax, id, word_bits) ? "" : "bignum expressions";
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
    return value.operation == ast::BinaryOperator::send ? "send expressions" : "arithmetic";
}
} // namespace erlang_aot::semantic
