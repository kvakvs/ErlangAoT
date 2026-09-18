#include "forms.hpp"

namespace erlang_aot {
ast::ExprId FormParser::make(ast::ExprValue value, std::size_t begin, std::size_t anchor) {
    node();
    return builder_.expression(std::move(value), builder_.source(begin, cursor_.offset(), anchor));
}

ast::ExprId FormParser::expression(int minimum) {
    if (depth_ >= nesting_) {
        fail(DiagnosticCode::resource_limit, "parser nesting budget exhausted");
    }
    ++depth_;
    auto left = prefix();
    while (const auto info = next_operator()) {
        if (info->precedence < minimum)
            break;
        left = continuation(std::move(left), *info);
        nonassociative(*info);
    }
    --depth_;
    return left;
}

ast::ExprId FormParser::prefix() {
    if (cursor_.empty())
        fail(DiagnosticCode::parser_syntax, "expected expression");
    const auto begin = cursor_.offset();
    if (cursor_.take_syntax(U"catch")) {
        auto operand = expression();
        return make(ast::CatchExpression{std::move(operand)}, begin, begin);
    }
    if (const auto info = prefix_operator(cursor_.anchor())) {
        cursor_.consume();
        auto operand = expression(info->precedence);
        return make(ast::UnaryExpression{info->operation, std::move(operand)}, begin, begin);
    }
    auto value = primary();
    return make(std::move(value), begin, begin);
}

std::optional<OperatorInfo> FormParser::next_operator() const {
    if (cursor_.empty())
        return std::nullopt;
    if (const auto call = call_operator(cursor_.anchor()))
        return call;
    return infix_operator(cursor_.anchor(), OperatorContext::expression);
}

ast::ExprId FormParser::continuation(ast::ExprId left, const OperatorInfo &info) {
    const auto begin = builder_.view().expression(left).source.begin;
    const auto anchor = cursor_.offset();
    cursor_.consume();
    if (info.spelling == U"(") {
        auto arguments = elements(U")");
        return make(ast::CallExpression{std::move(left), std::move(arguments)}, begin, anchor);
    }
    const auto minimum = info.precedence + (info.associativity == Associativity::right ? 0 : 1);
    auto right = expression(minimum);
    if (info.spelling == U"=") {
        return make(ast::MatchExpression{std::move(left), std::move(right)}, begin, anchor);
    }
    if (info.spelling == U":") {
        return make(ast::RemoteExpression{std::move(left), std::move(right)}, begin, anchor);
    }
    if (!info.operation) {
        throw std::logic_error("missing binary operator identity");
    }
    return make(ast::BinaryExpression{*info.operation, std::move(left), std::move(right)}, begin, anchor);
}

void FormParser::nonassociative(const OperatorInfo &info) const {
    if (info.associativity != Associativity::none)
        return;
    const auto next = next_operator();
    if (next && next->precedence == info.precedence) {
        fail(DiagnosticCode::parser_syntax, "nonassociative operators require parentheses");
    }
}
} // namespace erlang_aot
