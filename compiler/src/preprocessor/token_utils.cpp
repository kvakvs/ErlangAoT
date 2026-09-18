#include "token_utils.hpp"
#include <algorithm>
#include <array>
#include <charconv>

namespace erlang_aot {
bool syntax(const Token &token, std::u32string_view text) {
    return (token.kind == TokenKind::symbol || token.kind == TokenKind::keyword || token.kind == TokenKind::dot) &&
           token.text() == text;
}

Token generated(const Token &origin, TokenKind kind, TokenValue value) {
    Token result = origin;
    result.kind = kind;
    result.value = std::move(value);
    return result;
}

void pp_fail(DiagnosticCode code, std::string message, const Token &token) {
    throw Diagnostic{code, std::move(message), token.spelling, token.origins, Severity::error, token.location};
}

std::vector<Token> fragment(std::string text) {
    SourceManager sources;
    Lexer lexer(sources.add("<preprocessor>", std::move(text)));
    std::vector<Token> result;
    while (auto item = lexer.next()) {
        result.push_back(std::move(*item));
    }
    return result;
}

namespace {
// Erlang io_lib uses printable Latin-1 directly and named/control escapes otherwise.
std::u32string escaped(char32_t value, char32_t quote) {
    if (value == quote || value == U'\\') {
        return std::u32string{U'\\', value};
    }
    constexpr std::u32string_view controls = U"\n\r\t\v\b\f\x1b\x7f";
    constexpr std::u32string_view letters = U"nrtvbfed";
    const auto found = controls.find(value);
    if (found != std::u32string_view::npos) {
        return std::u32string{U'\\', letters[found]};
    }
    if (value >= 32 && (value < 127 || value >= 160)) {
        return std::u32string(1, value);
    }
    std::array<char, 16> digits{};
    const auto end = std::to_chars(digits.data(), digits.data() + digits.size(), static_cast<unsigned>(value), 8).ptr;
    std::u32string result = U"\\";
    result.append(digits.data(), end);
    return result;
}

// Quote decoded values; original whitespace and literal escapes are not preserved by epp.
std::u32string quoted(std::u32string_view text, char32_t quote) {
    std::u32string result(1, quote);
    for (const auto item : text) {
        result += escaped(item, quote);
    }
    result += quote;
    return result;
}

// Canonical floats use Erlang's scientific presentation used by io_lib:format("~w").
std::u32string floating_text(double value) {
    std::array<char, 1200> data{};
    const auto end = std::to_chars(data.data(), data.data() + data.size(), value, std::chars_format::scientific).ptr;
    std::string text(data.data(), end);
    const auto e = text.find('e');
    if (text.find('.') == std::string::npos) {
        text.insert(e, ".0");
    }
    auto exponent = text.find('e') + 1;
    if (text[exponent] == '+') {
        text.erase(exponent, 1);
    }
    if (text[exponent] == '-') {
        ++exponent;
    }
    while (text.size() - exponent > 1 && text[exponent] == '0') {
        text.erase(exponent, 1);
    }
    const auto fixed_end = std::to_chars(data.data(), data.data() + data.size(), value, std::chars_format::fixed).ptr;
    std::string fixed(data.data(), fixed_end);
    if (fixed.find('.') == std::string::npos) {
        fixed += ".0";
    }
    if (fixed.size() <= text.size()) {
        text = std::move(fixed);
    }
    return {text.begin(), text.end()};
}

// Atoms that coincide with reserved words need quotes even when their letters are ordinary.
std::u32string atom_text(std::u32string_view text) {
    if (text.empty() || !atom_start(text.front()) || word_length(text) != text.size()) {
        return quoted(text, U'\'');
    }
    if (fragment(utf8(text)).front().kind == TokenKind::keyword) {
        return quoted(text, U'\'');
    }
    return std::u32string(text);
}
} // namespace

std::u32string token_text(const Token &token) {
    if (const auto *integer = std::get_if<Integer>(&token.value)) {
        if (token.kind == TokenKind::character) {
            return U"$" + escaped(static_cast<char32_t>(std::stoul(integer->decimal)), U'\'');
        }
        return {integer->decimal.begin(), integer->decimal.end()};
    }
    if (const auto *number = std::get_if<double>(&token.value)) {
        return floating_text(*number);
    }
    if (token.kind == TokenKind::string) {
        return quoted(token.text(), U'"');
    }
    if (token.kind == TokenKind::atom) {
        return atom_text(token.text());
    }
    return std::u32string(token.text());
}

std::u32string stringify(std::span<const Token> tokens) {
    std::u32string result;
    for (const auto &token : tokens) {
        if (!result.empty()) {
            result += U' ';
        }
        result += token_text(token);
    }
    return result;
}
} // namespace erlang_aot
