#include "forms.hpp"
#include <charconv>

namespace erlang_aot {
namespace {
// Character tokens use the lexer's arbitrary integer representation, not text values.
ast::CharacterLiteral character(const Integer &integer, const Token &token) {
    std::uint32_t codepoint = 0;
    const auto *end = integer.decimal.data() + integer.decimal.size();
    const auto result = std::from_chars(integer.decimal.data(), end, codepoint);
    if (result.ec != std::errc{} || result.ptr != end || codepoint > 0x10ffff) {
        throw token_diagnostic(DiagnosticCode::parser_contract, "invalid character token", token);
    }
    return {static_cast<char32_t>(codepoint)};
}
} // namespace

ast::ExprValue FormParser::literal_value(const Token &token) const {
    switch (token.kind) {
    case TokenKind::atom:
        return ast::Atom{value<std::u32string>(token)};
    case TokenKind::integer:
        return ast::IntegerLiteral{value<Integer>(token)};
    case TokenKind::floating:
        return ast::FloatLiteral{value<double>(token)};
    case TokenKind::string:
        return ast::StringLiteral{value<std::u32string>(token)};
    case TokenKind::character:
        return character(value<Integer>(token), token);
    default:
        throw token_diagnostic(DiagnosticCode::unsupported_syntax, "only scalar literals are implemented in Phase I",
                               token);
    }
}

ast::ExprId FormParser::literal() {
    if (cursor_.empty() || cursor_.anchor().kind == TokenKind::dot) {
        fail(DiagnosticCode::parser_syntax, "expected function body expression");
    }
    const auto begin = cursor_.offset();
    auto result = literal_value(*cursor_.consume());
    node();
    return builder_.expression(std::move(result), builder_.source(begin, cursor_.offset(), begin));
}
} // namespace erlang_aot
