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

// try ... catch ... after runs; named stacktrace variables (plan step 15) stay deferred.
std::string_view ExpressionCapability::operator()(const ast::TryExpression &value) const {
    const auto named_stack = [this](const ast::CatchClause &clause) {
        return clause.stacktrace && std::get<ast::Variable>(syntax.expression(*clause.stacktrace).value).name != U"_";
    };
    if (value.handlers && std::ranges::any_of(*value.handlers, named_stack)) {
        return "exceptions";
    }
    return {};
}

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
