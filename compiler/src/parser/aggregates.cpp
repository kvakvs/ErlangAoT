#include "forms.hpp"

namespace erlang_aot {
ast::ExprValue FormParser::primary(OperatorContext context) {
    if (auto value = control(context))
        return std::move(*value);
    if (cursor_.empty() || cursor_.anchor().kind == TokenKind::dot) {
        fail(DiagnosticCode::parser_syntax, "expected expression");
    }
    if (cursor_.take_syntax(U"(")) {
        auto child = expression(0, context);
        expect(U")");
        return ast::Group{std::move(child)};
    }
    if (cursor_.take_syntax(U"{")) {
        return tuple();
    }
    if (cursor_.take_syntax(U"[")) {
        return list(context != OperatorContext::pattern);
    }
    if (cursor_.take_syntax(U"<<")) {
        return binary(context != OperatorContext::pattern);
    }
    if (cursor_.anchor().kind == TokenKind::sigil_prefix) {
        return sigil();
    }
    if (syntax(cursor_.anchor(), U"#"))
        return maximum_hash();
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

ast::ExprValue FormParser::list(bool comprehension) {
    ast::List result;
    if (cursor_.take_syntax(U"]")) {
        return result;
    }
    do {
        result.elements.push_back(expression());
    } while (cursor_.take_syntax(U","));
    if (cursor_.take_syntax(U"||")) {
        require_comprehension(comprehension);
        auto items = qualifiers();
        expect(U"]");
        return ast::ListComprehension{std::move(result.elements), std::move(items)};
    }
    if (cursor_.take_syntax(U"|")) {
        result.tail = expression();
    }
    expect(U"]");
    return result;
}
} // namespace erlang_aot
