#include "value.hpp"
#include <algorithm>

namespace erlang_aot {
namespace {
// Append punctuation and recursively normalized term tokens at one definition site.
void symbol(std::vector<Token> &output, const Token &site, std::u32string_view text) {
    output.push_back(generated(site, TokenKind::symbol, std::u32string(text)));
}

bool character_value(const Value &value) {
    if (value.kind != ValueKind::integer || value.integer < 0 || value.integer > 0x10ffff) {
        return false;
    }
    return value.integer < 0xd800 || value.integer > 0xdfff;
}

// Erlang abstracts proper lists of Unicode scalar integers as string tokens.
bool emit_string(const Value &value, const Token &site, std::vector<Token> &output) {
    if (value.tail || !std::ranges::all_of(value.elements, character_value)) {
        return false;
    }
    std::u32string text;
    for (const auto &element : value.elements) {
        text += static_cast<char32_t>(element.integer.convert_to<unsigned>());
    }
    output.push_back(generated(site, TokenKind::string, std::move(text)));
    return true;
}

void append(std::vector<Token> &output, const Value &value, const Token &site) {
    auto tokens = term_tokens(value, site);
    output.insert(output.end(), std::make_move_iterator(tokens.begin()), std::make_move_iterator(tokens.end()));
}

void sequence(std::vector<Token> &output, const Value &value, const Token &site) {
    const bool tuple = value.kind == ValueKind::tuple;
    symbol(output, site, tuple ? U"{" : U"[");
    for (std::size_t i = 0; i < value.elements.size(); ++i) {
        if (i != 0) {
            symbol(output, site, U",");
        }
        append(output, value.elements[i], site);
    }
    if (value.tail) {
        symbol(output, site, U"|");
        append(output, *value.tail, site);
    }
    symbol(output, site, tuple ? U"}" : U"]");
}

// Stable exact-key order makes initial definitions independent of map insertion order.
void map_tokens(std::vector<Token> &output, const Value &value, const Token &site) {
    std::vector<std::size_t> keys;
    for (std::size_t i = 0; i < value.elements.size(); i += 2) {
        keys.push_back(i);
    }
    std::ranges::sort(
        keys, [&](std::size_t a, std::size_t b) { return compare(value.elements[a], value.elements[b], true) < 0; });
    symbol(output, site, U"#");
    symbol(output, site, U"{");
    bool first = true;
    for (const auto key : keys) {
        if (!first) {
            symbol(output, site, U",");
        }
        first = false;
        append(output, value.elements[key], site);
        symbol(output, site, U"=>");
        append(output, value.elements[key + 1], site);
    }
    symbol(output, site, U"}");
}

void binary_tokens(std::vector<Token> &output, const Value &value, const Token &site) {
    symbol(output, site, U"<<");
    for (std::size_t start = 0; start < value.bits.size(); start += 8) {
        if (start != 0) {
            symbol(output, site, U",");
        }
        const auto count = std::min<std::size_t>(8, value.bits.size() - start);
        unsigned byte = 0;
        for (std::size_t bit = 0; bit < count; ++bit) {
            byte = byte * 2 + static_cast<unsigned>(value.bits[start + bit]);
        }
        output.push_back(generated(site, TokenKind::integer, Integer{std::to_string(byte)}));
        if (count != 8) {
            symbol(output, site, U":");
            output.push_back(generated(site, TokenKind::integer, Integer{std::to_string(count)}));
        }
    }
    symbol(output, site, U">>");
}

void compound(std::vector<Token> &output, const Value &value, const Token &site) {
    switch (value.kind) {
    case ValueKind::map:
        map_tokens(output, value, site);
        return;
    case ValueKind::bits:
        binary_tokens(output, value, site);
        return;
    case ValueKind::function:
        output.push_back(generated(site, TokenKind::keyword, std::u32string(U"fun")));
        output.push_back(generated(site, TokenKind::atom, value.text));
        symbol(output, site, U":");
        append(output, value.elements[0], site);
        symbol(output, site, U"/");
        append(output, value.elements[1], site);
        return;
    case ValueKind::list:
        if (emit_string(value, site, output)) {
            return;
        }
        break;
    default:
        break;
    }
    sequence(output, value, site);
}
} // namespace

std::vector<Token> term_tokens(const Value &value, const Token &site) {
    switch (value.kind) {
    case ValueKind::integer:
        return {generated(site, TokenKind::integer, Integer{value.integer.str()})};
    case ValueKind::floating:
        return {generated(site, TokenKind::floating, value.real)};
    case ValueKind::atom:
        return {generated(site, TokenKind::atom, value.text)};
    default: {
        std::vector<Token> result;
        compound(result, value, site);
        return result;
    }
    }
}

std::string display(const Value &value) {
    const auto tokens = term_tokens(value, Token{});
    return utf8(stringify(tokens));
}
} // namespace erlang_aot
