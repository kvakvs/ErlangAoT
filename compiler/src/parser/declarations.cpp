#include "attribute_values.hpp"
#include "forms.hpp"

namespace erlang_aot {
ast::RecordDeclaration FormParser::record_declaration() {
    const auto enclosed = cursor_.take_syntax(U"(");
    const auto native = cursor_.take_syntax(U"#");
    auto name = native ? record_name() : ast::Atom{value<std::u32string>(category(TokenKind::atom, "record name"))};
    if (!native)
        expect(U",");
    expect(U"{");
    ast::RecordDeclaration result{std::move(name), native, {}};
    if (!cursor_.take_syntax(U"}")) {
        do {
            result.fields.push_back(declaration_field());
        } while (cursor_.take_syntax(U","));
        expect(U"}");
    }
    if (enclosed)
        expect(U")");
    return result;
}

ast::RecordDeclarationField FormParser::declaration_field() {
    const auto begin = cursor_.offset();
    auto field = expression();
    const auto &expression = ungroup(builder_.view(), field);
    auto name = field;
    std::optional<ast::ExprId> default_value;
    if (const auto *match = std::get_if<ast::MatchExpression>(&expression.value)) {
        name = match->left;
        default_value = match->right;
    }
    const auto *atom = std::get_if<ast::Atom>(&ungroup(builder_.view(), name).value);
    if (!atom)
        fail(DiagnosticCode::parser_syntax, "bad record field");
    return {*atom, std::move(default_value), builder_.source(begin, cursor_.offset(), begin)};
}
} // namespace erlang_aot
