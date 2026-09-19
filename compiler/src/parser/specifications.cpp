#include "forms.hpp"

namespace erlang_aot {
ast::Specification FormParser::specification(bool callback) {
    const auto enclosed = cursor_.take_syntax(U"(");
    auto name = ast::Atom{value<std::u32string>(category(TokenKind::atom, "specification function name"))};
    std::optional<ast::Atom> module;
    if (cursor_.take_syntax(U":")) {
        module = std::move(name);
        name = ast::Atom{value<std::u32string>(category(TokenKind::atom, "qualified function name"))};
    }
    std::vector<ast::SpecificationSignature> signatures;
    do {
        signatures.push_back(signature());
    } while (cursor_.take_syntax(U";"));
    if (enclosed) {
        expect(U")");
    }
    const auto &arguments = signatures.front().function.arguments;
    if (!arguments) {
        fail(DiagnosticCode::parser_syntax, "first specification requires a fixed argument product");
    }
    return {callback, std::move(module), std::move(name), arguments->size(), std::move(signatures)};
}

ast::SpecificationSignature FormParser::signature() {
    const auto begin = cursor_.offset();
    auto function = fun_type();
    std::vector<ast::TypeConstraint> constraints;
    if (cursor_.take_syntax(U"when")) {
        do {
            constraints.push_back(constraint());
        } while (cursor_.take_syntax(U","));
    }
    return {std::move(function), std::move(constraints), builder_.source(begin, cursor_.offset(), begin)};
}

ast::TypeConstraint FormParser::constraint() {
    const auto begin = cursor_.offset();
    if (cursor_.anchor().kind == TokenKind::atom) {
        return legacy_constraint(begin);
    }
    auto variable = ast::Variable{value<std::u32string>(category(TokenKind::variable, "constraint variable"))};
    if (variable.name == U"_") {
        fail(DiagnosticCode::parser_syntax, "bad type variable");
    }
    expect(U"::");
    auto bound = top_type();
    return {std::move(variable), std::move(bound), false, builder_.source(begin, cursor_.offset(), begin)};
}

ast::TypeConstraint FormParser::legacy_constraint(std::size_t begin) {
    const auto &name = category(TokenKind::atom, "constraint name");
    if (name.text() != U"is_subtype") {
        fail(DiagnosticCode::parser_syntax, "unsupported legacy constraint");
    }
    expect(U"(");
    auto arguments = type_elements(U")");
    if (arguments.size() != 2) {
        fail(DiagnosticCode::parser_syntax, "legacy subtype constraint requires two arguments");
    }
    auto id = arguments.front();
    while (const auto *group = std::get_if<ast::TypeGroup>(&builder_.view().type(id).value)) {
        id = group->type;
    }
    const auto *variable = std::get_if<ast::Variable>(&builder_.view().type(id).value);
    if (!variable || variable->name == U"_") {
        fail(DiagnosticCode::parser_syntax, "bad type variable");
    }
    return {*variable, arguments[1], true, builder_.source(begin, cursor_.offset(), begin)};
}
} // namespace erlang_aot
