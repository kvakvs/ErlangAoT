#include "forms.hpp"

namespace erlang_aot {
ast::ExprId FormParser::make(ast::ExprValue value, std::size_t begin, std::size_t anchor) {
    node();
    return builder_.expression(std::move(value), builder_.source(begin, cursor_.offset(), anchor));
}

void FormParser::enter() {
    work();
    if (depth_ >= nesting_) {
        fail(DiagnosticCode::resource_limit, "parser nesting budget exhausted");
    }
    ++depth_;
}

ast::ExprId FormParser::expression(int minimum, OperatorContext context) {
    enter();
    auto left = prefix(context);
    while (const auto info = next_operator(context)) {
        if (info->precedence < minimum)
            break;
        left = continuation(std::move(left), *info, context);
        nonassociative(*info, context);
    }
    --depth_;
    return left;
}

ast::ExprId FormParser::prefix(OperatorContext context) {
    if (cursor_.empty())
        fail(DiagnosticCode::parser_syntax, "expected expression");
    const auto begin = cursor_.offset();
    if (context != OperatorContext::pattern && cursor_.take_syntax(U"catch")) {
        auto operand = expression();
        return make(ast::CatchExpression{std::move(operand)}, begin, begin);
    }
    if (const auto info = prefix_operator(cursor_.anchor())) {
        cursor_.consume();
        auto operand = expression(info->precedence, context);
        return make(ast::UnaryExpression{info->operation, std::move(operand)}, begin, begin);
    }
    return structural(context);
}

std::optional<OperatorInfo> FormParser::next_operator(OperatorContext context) const {
    if (cursor_.empty())
        return std::nullopt;
    if (context != OperatorContext::pattern) {
        if (const auto call = call_operator(cursor_.anchor()))
            return call;
    }
    return infix_operator(cursor_.anchor(), context);
}

ast::ExprId FormParser::continuation(ast::ExprId left, const OperatorInfo &info, OperatorContext context) {
    const auto begin = builder_.view().expression(left).source.begin;
    const auto anchor = cursor_.offset();
    cursor_.consume();
    if (info.spelling == U"(") {
        auto arguments = elements(U")");
        return make(ast::CallExpression{std::move(left), std::move(arguments)}, begin, anchor);
    }
    const auto minimum = info.precedence + (info.associativity == Associativity::right ? 0 : 1);
    auto right = expression(minimum, context);
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

void FormParser::nonassociative(const OperatorInfo &info, OperatorContext context) const {
    if (info.associativity != Associativity::none)
        return;
    const auto next = next_operator(context);
    if (next && next->precedence == info.precedence) {
        fail(DiagnosticCode::parser_syntax, "nonassociative operators require parentheses");
    }
}
} // namespace erlang_aot
