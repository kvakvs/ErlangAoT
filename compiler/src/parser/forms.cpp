#include "forms.hpp"

namespace erlang_aot {
FormParser::FormParser(std::span<const Token> tokens, const Token &end, ast::Builder &builder, std::size_t nodes)
    : cursor_(tokens, end), builder_(builder), nodes_(nodes) {}

void FormParser::fail(DiagnosticCode code, std::string message) const {
    throw token_diagnostic(code, std::move(message), cursor_.anchor());
}

void FormParser::expect(std::u32string_view text) {
    if (!cursor_.take(TokenKind::symbol, text)) {
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
        fail(DiagnosticCode::unsupported_syntax, "syntax beyond a single scalar body is not implemented in Phase I");
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
    const auto &name = category(TokenKind::atom, "attribute name");
    if (name.text() == U"module") {
        return module_attribute();
    }
    if (name.text() == U"file") {
        return file_attribute();
    }
    throw token_diagnostic(DiagnosticCode::unsupported_syntax, "attribute syntax is not implemented in Phase I", name);
}

ast::ModuleAttribute FormParser::module_attribute() {
    expect(U"(");
    ast::ModuleAttribute result{{value<std::u32string>(category(TokenKind::atom, "module atom"))}};
    expect(U")");
    return result;
}

ast::FileAttribute FormParser::file_attribute() {
    expect(U"(");
    auto name = value<std::u32string>(category(TokenKind::string, "filename string"));
    expect(U",");
    auto line = value<Integer>(category(TokenKind::integer, "file line integer"));
    expect(U")");
    return {std::move(name), std::move(line)};
}

ast::ZeroArgumentFunction FormParser::function() {
    ast::Atom name{value<std::u32string>(category(TokenKind::atom, "function name"))};
    expect(U"(");
    if (!cursor_.take(TokenKind::symbol, U")")) {
        fail(DiagnosticCode::unsupported_syntax, "only zero-argument functions are implemented in Phase I");
    }
    expect(U"->");
    auto result = literal();
    return {std::move(name), {std::move(result)}};
}
} // namespace erlang_aot
