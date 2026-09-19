#include "attribute_values.hpp"
#include "forms.hpp"
#include <algorithm>

namespace erlang_aot {
ast::RecordDeclaration FormParser::record_declaration() {
    const auto enclosed = cursor_.take_syntax(U"(");
    const auto native = cursor_.take_syntax(U"#");
    auto name = native ? record_name() : ast::Atom{value<std::u32string>(category(TokenKind::atom, "record name"))};
    if (!native)
        expect(U",");
    ast::RecordDeclaration result{std::move(name), native, declaration_fields()};
    if (enclosed)
        expect(U")");
    return result;
}

std::vector<ast::RecordDeclarationField> FormParser::declaration_fields() {
    std::size_t groups = 0;
    while (cursor_.take_syntax(U"(")) {
        enter();
        ++groups;
    }
    expect(U"{");
    std::vector<ast::RecordDeclarationField> result;
    if (!cursor_.take_syntax(U"}")) {
        do {
            result.push_back(declaration_field());
        } while (cursor_.take_syntax(U","));
        expect(U"}");
    }
    const auto typed = std::ranges::any_of(result, [](const auto &field) { return field.type.has_value(); });
    if (groups != 0 && typed)
        fail(DiagnosticCode::parser_syntax, "typed record fields require an ungrouped tuple");
    while (groups != 0) {
        expect(U")");
        --groups;
        --depth_;
    }
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
    const auto field_name = *atom;
    std::optional<ast::TypeId> type;
    if (cursor_.take_syntax(U"::"))
        type = top_type();
    return {field_name, std::move(default_value), builder_.source(begin, cursor_.offset(), begin), std::move(type)};
}

ast::TypeDeclaration FormParser::type_declaration(const ast::Atom &name, const ast::ExprId &head) {
    ast::TypeDeclarationKind kind;
    if (name.name == U"type")
        kind = ast::TypeDeclarationKind::alias;
    else if (name.name == U"opaque")
        kind = ast::TypeDeclarationKind::opaque;
    else if (name.name == U"nominal")
        kind = ast::TypeDeclarationKind::nominal;
    else
        fail(DiagnosticCode::parser_syntax, "bad typed attribute");
    try {
        const auto &call = attribute_as<ast::CallExpression>(builder_.view(), head);
        auto type_name = attribute_as<ast::Atom>(builder_.view(), call.target);
        std::vector<ast::Variable> parameters;
        for (const auto &id : call.arguments) {
            auto variable = attribute_as<ast::Variable>(builder_.view(), id);
            if (variable.name == U"_")
                fail(DiagnosticCode::parser_syntax, "bad type variable");
            parameters.push_back(std::move(variable));
        }
        auto type = top_type();
        return {kind, std::move(type_name), std::move(parameters), std::move(type)};
    } catch (const EvaluationFailure &) {
        fail(DiagnosticCode::parser_syntax, "bad type declaration");
    }
}
} // namespace erlang_aot
