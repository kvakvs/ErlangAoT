#include "forms.hpp"

namespace erlang_aot {
namespace {
// Type-only punctuation is handled before requiring a shared arithmetic identity.
ast::BinaryOperator type_operation(const OperatorInfo &info) {
    if (!info.operation) {
        throw std::logic_error("missing type operator identity");
    }
    return *info.operation;
}
} // namespace

ast::TypeId FormParser::make_type(ast::TypeValue value, std::size_t begin, std::size_t anchor) {
    node();
    return builder_.type(std::move(value), builder_.source(begin, cursor_.offset(), anchor));
}

ast::TypeId FormParser::top_type() {
    enter();
    const auto begin = cursor_.offset();
    const auto *next = cursor_.peek(1);
    if (cursor_.anchor().kind == TokenKind::variable && next && syntax(*next, U"::")) {
        ast::Variable variable{value<std::u32string>(*cursor_.consume())};
        cursor_.consume();
        auto type = top_type();
        --depth_;
        return make_type(ast::AnnotatedType{std::move(variable), std::move(type)}, begin, begin);
    }
    auto left = type_expression();
    if (cursor_.take_syntax(U"|")) {
        auto right = top_type();
        left = make_type(ast::UnionType{std::move(left), std::move(right)}, begin, begin);
    }
    --depth_;
    return left;
}

ast::TypeId FormParser::type_expression(int minimum) {
    enter();
    const auto begin = cursor_.offset();
    auto left = type_prefix();
    while (const auto info = infix_operator(cursor_.anchor(), OperatorContext::type)) {
        if (info->precedence < minimum) {
            break;
        }
        const auto anchor = cursor_.offset();
        cursor_.consume();
        auto right = type_expression(info->precedence + 1);
        if (info->spelling == U"..") {
            left = make_type(ast::RangeType{std::move(left), std::move(right)}, begin, anchor);
        } else {
            left = make_type(ast::BinaryTypeOperator{type_operation(*info), std::move(left), std::move(right)}, begin,
                             anchor);
        }
        const auto next = infix_operator(cursor_.anchor(), OperatorContext::type);
        if (info->associativity == Associativity::none && next && next->precedence == info->precedence) {
            fail(DiagnosticCode::parser_syntax, "nonassociative type operator requires parentheses");
        }
    }
    --depth_;
    return left;
}

ast::TypeId FormParser::type_prefix() {
    const auto begin = cursor_.offset();
    if (const auto operation = prefix_operator(cursor_.anchor())) {
        cursor_.consume();
        auto operand = type_expression(operation->precedence);
        return make_type(ast::UnaryType{operation->operation, std::move(operand)}, begin, begin);
    }
    return make_type(type_primary(), begin, begin);
}

std::vector<ast::TypeId> FormParser::type_elements(std::u32string_view close) {
    std::vector<ast::TypeId> result;
    if (cursor_.take_syntax(close)) {
        return result;
    }
    do {
        result.push_back(top_type());
    } while (cursor_.take_syntax(U","));
    expect(close);
    return result;
}
} // namespace erlang_aot
