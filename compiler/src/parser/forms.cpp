#include "forms.hpp"

namespace erlang_aot {
FormParser::FormParser(std::span<const Token> tokens, const Token &end, ast::Builder &builder, GrammarBudget budget)
    : cursor_(tokens, end), builder_(builder), nodes_(budget.nodes), nesting_(budget.nesting) {}

void FormParser::fail(DiagnosticCode code, std::string message) const {
    throw token_diagnostic(code, std::move(message), cursor_.anchor());
}

void FormParser::expect(std::u32string_view text) {
    if (cursor_.anchor().kind == TokenKind::dot || !cursor_.take_syntax(text)) {
        fail(DiagnosticCode::parser_syntax, "expected '" + utf8(text) + "'");
    }
}

const Token &FormParser::category(TokenKind kind, std::string_view description) {
    if (cursor_.empty() || cursor_.anchor().kind != kind) {
        fail(DiagnosticCode::parser_syntax, "expected " + std::string(description));
    }
    return *cursor_.consume();
}

void FormParser::node() {
    if (nodes_ == 0) {
        fail(DiagnosticCode::resource_limit, "parser AST node budget exhausted");
    }
    --nodes_;
}

void FormParser::terminator() {
    if (cursor_.empty()) {
        fail(DiagnosticCode::missing_terminator, "expected form-ending '.'");
    }
    if (!cursor_.take(TokenKind::dot, U".")) {
        fail(DiagnosticCode::parser_syntax, "expected form-ending dot");
    }
    if (!cursor_.empty()) {
        fail(DiagnosticCode::parser_syntax, "unexpected tokens after form-ending '.'");
    }
}

ast::FormId FormParser::parse() {
    if (cursor_.empty()) {
        fail(DiagnosticCode::parser_syntax, "expected an Erlang form");
    }
    auto result = cursor_.take(TokenKind::symbol, U"-") ? attribute() : ast::FormValue(function());
    terminator();
    node();
    return builder_.form(std::move(result), builder_.source(0, cursor_.offset(), 0));
}

ast::FormValue FormParser::attribute() {
    auto name = ast::Atom{value<std::u32string>(category(TokenKind::atom, "attribute name"))};
    if (name.name == U"record")
        return record_declaration();
    if (name.name == U"spec" || name.name == U"callback")
        return specification(name.name == U"callback");
    const auto enclosed = cursor_.take_syntax(U"(");
    const auto checkpoint = builder_.view().expression_count();
    auto arguments = sequence();
    const bool closed = enclosed && cursor_.take_syntax(U")");
    if (closed && cursor_.take_syntax(U",")) {
        auto rest = sequence();
        arguments.insert(arguments.end(), rest.begin(), rest.end());
    }
    auto result = attribute_body(std::move(name), arguments);
    if (enclosed && !closed)
        expect(U")");
    if (!std::holds_alternative<ast::DocumentationAttribute>(result))
        builder_.discard_expressions(checkpoint);
    return result;
}

ast::FormValue FormParser::attribute_body(ast::Atom name, const std::vector<ast::ExprId> &arguments) {
    if (!cursor_.take_syntax(U"::"))
        return ordinary_attribute(std::move(name), arguments);
    if (arguments.size() != 1)
        fail(DiagnosticCode::parser_syntax, "bad type declaration head");
    return type_declaration(name, arguments.front());
}

} // namespace erlang_aot
