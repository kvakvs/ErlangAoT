#include "capabilities.hpp"
#include <charconv>
#include <erlang_aot/abi/term.hpp>
#include <limits>

namespace erlang_aot::semantic {
ast::ExprId ungroup(const ast::Module &syntax, ast::ExprId expression) {
    while (const auto *group = std::get_if<ast::Group>(&syntax.expression(expression).value)) {
        expression = group->expression;
    }
    return expression;
}

namespace {
// Share ABI v1 bounds with runtime/codegen while choosing the eventual target width explicitly.
bool fits_target(const std::int64_t number, const unsigned bits) {
    if (bits == 32) {
        return abi::v1::IntegerEncoding<32>::encode(number).has_value();
    }
    return bits == 64 && abi::v1::IntegerEncoding<64>::encode(number).has_value();
}

// Literal syntax can contain a character code or a canonical decimal integer.
std::optional<std::int64_t> literal_value(const ast::ExprValue &value) {
    if (const auto *character = std::get_if<ast::CharacterLiteral>(&value)) {
        return static_cast<std::int64_t>(character->value);
    }
    const auto *integer = std::get_if<ast::IntegerLiteral>(&value);
    if (!integer) {
        return {};
    }
    const auto &text = integer->value.decimal;
    std::int64_t result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        return {};
    }
    return result;
}
} // namespace

std::optional<std::int64_t> integer_literal(const ast::Module &syntax, ast::ExprId expression,
                                            const unsigned word_bits) {
    if (word_bits != 32 && word_bits != 64) {
        return {};
    }
    expression = ungroup(syntax, expression);
    const auto &value = syntax.expression(expression).value;
    const auto *unary = std::get_if<ast::UnaryExpression>(&value);
    auto number = literal_value(value);
    if (unary && unary->operation == ast::UnaryOperator::negative) {
        number = literal_value(syntax.expression(ungroup(syntax, unary->operand)).value);
        if (number && *number != std::numeric_limits<std::int64_t>::min()) {
            number = -*number;
        } else {
            return {};
        }
    }
    if (!number || !fits_target(*number, word_bits)) {
        return {};
    }
    return number;
}

std::vector<ast::ExprId> expression_children(const ast::Expression &expression) {
    if (const auto *tuple = std::get_if<ast::Tuple>(&expression.value)) {
        return tuple->elements;
    }
    if (const auto *list = std::get_if<ast::List>(&expression.value)) {
        auto children = list->elements;
        if (list->tail) {
            children.push_back(*list->tail);
        }
        return children;
    }
    if (const auto *group = std::get_if<ast::Group>(&expression.value)) {
        return {group->expression};
    }
    if (const auto *call = std::get_if<ast::CallExpression>(&expression.value)) {
        return call->arguments;
    }
    if (const auto *match = std::get_if<ast::MatchExpression>(&expression.value)) {
        return {match->right};
    }
    if (const auto *binary = std::get_if<ast::BinaryExpression>(&expression.value)) {
        return {binary->left, binary->right};
    }
    if (const auto *unary = std::get_if<ast::UnaryExpression>(&expression.value)) {
        return {unary->operand};
    }
    return {};
}

std::vector<ast::ExprId> function_roots(const ast::Function &function) {
    std::vector<ast::ExprId> result;
    for (const auto &clause : function.clauses) {
        if (clause.guard) {
            for (const auto &alternative : clause.guard->alternatives) {
                result.insert(result.end(), alternative.tests.begin(), alternative.tests.end());
            }
        }
        result.insert(result.end(), clause.body.begin(), clause.body.end());
    }
    return result;
}
} // namespace erlang_aot::semantic
