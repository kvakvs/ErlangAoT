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
    case TokenKind::variable:
        return ast::Variable{value<std::u32string>(token)};
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
        throw token_diagnostic(DiagnosticCode::unsupported_syntax, "expected a supported expression", token);
    }
}

ast::ExprValue FormParser::literal() {
    auto result = literal_value(*cursor_.consume());
    auto *string = std::get_if<ast::StringLiteral>(&result);
    if (string) {
        while (!cursor_.empty() && cursor_.anchor().kind == TokenKind::string) {
            string->value += value<std::u32string>(*cursor_.consume());
        }
    }
    return result;
}

ast::ExprValue FormParser::sigil() {
    const auto &prefix = category(TokenKind::sigil_prefix, "sigil prefix");
    auto content = value<std::u32string>(category(TokenKind::string, "sigil content"));
    const auto &suffix = category(TokenKind::sigil_suffix, "sigil suffix");
    if (!value<std::u32string>(suffix).empty()) {
        throw token_diagnostic(DiagnosticCode::parser_syntax, "illegal sigil suffix", suffix);
    }
    const auto &name = value<std::u32string>(prefix);
    if (name == U"s" || name == U"S") {
        return ast::StringLiteral{std::move(content)};
    }
    if (name.empty() || name == U"b" || name == U"B") {
        return ast::BinarySigilLiteral{std::move(content)};
    }
    throw token_diagnostic(DiagnosticCode::parser_syntax, "illegal sigil prefix", prefix);
}
} // namespace erlang_aot
