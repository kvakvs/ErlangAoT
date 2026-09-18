#include "forms.hpp"

namespace erlang_aot {
ast::ExprId FormParser::expression() {
    if (depth_ >= nesting_) {
        fail(DiagnosticCode::resource_limit, "parser nesting budget exhausted");
    }
    ++depth_;
    const auto begin = cursor_.offset();
    auto result = primary();
    node();
    --depth_;
    return builder_.expression(std::move(result), builder_.source(begin, cursor_.offset(), begin));
}

ast::ExprValue FormParser::primary() {
    if (cursor_.empty() || cursor_.anchor().kind == TokenKind::dot) {
        fail(DiagnosticCode::parser_syntax, "expected expression");
    }
    if (cursor_.take_syntax(U"(")) {
        auto child = expression();
        expect(U")");
        return ast::Group{std::move(child)};
    }
    if (cursor_.take_syntax(U"{")) {
        return tuple();
    }
    if (cursor_.take_syntax(U"[")) {
        return list();
    }
    if (cursor_.anchor().kind == TokenKind::sigil_prefix) {
        return sigil();
    }
    return literal();
}

std::vector<ast::ExprId> FormParser::elements(std::u32string_view close) {
    std::vector<ast::ExprId> result;
    if (cursor_.take_syntax(close)) {
        return result;
    }
    do {
        result.push_back(expression());
    } while (cursor_.take_syntax(U","));
    expect(close);
    return result;
}

ast::Tuple FormParser::tuple() { return {elements(U"}")}; }

ast::List FormParser::list() {
    ast::List result;
    if (cursor_.take_syntax(U"]")) {
        return result;
    }
    do {
        result.elements.push_back(expression());
    } while (cursor_.take_syntax(U","));
    if (cursor_.take_syntax(U"|")) {
        result.tail = expression();
    }
    expect(U"]");
    return result;
}
} // namespace erlang_aot
