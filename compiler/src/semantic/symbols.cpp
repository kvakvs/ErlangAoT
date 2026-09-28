#include "symbols.hpp"
#include <charconv>
#include <stdexcept>

namespace erlang_aot::semantic {
namespace {
// Only ASCII hex enters native symbols, including for quoted names and delimiter bytes.
std::string hex(std::string_view text) {
    constexpr std::string_view digits = "0123456789abcdef";
    std::string result;
    for (const unsigned char byte : text) {
        result += digits[byte >> 4U];
        result += digits[byte & 15U];
    }
    return result;
}

// Decode exactly two lowercase hex digits per byte; reject noncanonical components.
std::optional<std::string> unhex(std::string_view text) {
    if (text.size() % 2 != 0) {
        return {};
    }
    constexpr std::string_view digits = "0123456789abcdef";
    std::string result;
    for (std::size_t i = 0; i < text.size(); i += 2) {
        const auto high = digits.find(text[i]);
        const auto low = digits.find(text[i + 1]);
        if (high == digits.npos || low == digits.npos) {
            return {};
        }
        result += static_cast<char>(high * 16 + low);
    }
    return result;
}

// Require a canonical, bounded arity component before forming a decoded identity.
std::optional<std::size_t> symbol_arity(std::string_view text) {
    std::size_t count = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), count);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || count > 255) {
        return {};
    }
    if (std::to_string(count) != text) {
        return {};
    }
    return count;
}

} // namespace

std::string encode_symbol(const SymbolIdentity &identity) {
    if (identity.arity > 255) {
        throw std::invalid_argument("symbol arity exceeds 255");
    }
    return "eav1_" + hex(identity.module) + "_" + hex(identity.function) + "_" + std::to_string(identity.arity);
}

std::optional<SymbolIdentity> decode_symbol(std::string_view symbol) {
    if (!symbol.starts_with("eav1_")) {
        return {};
    }
    symbol.remove_prefix(5);
    const auto first = symbol.find('_');
    const auto second = symbol.find('_', first == symbol.npos ? first : first + 1);
    if (second == symbol.npos) {
        return {};
    }
    const auto module = unhex(symbol.substr(0, first));
    const auto function = unhex(symbol.substr(first + 1, second - first - 1));
    const auto count = symbol_arity(symbol.substr(second + 1));
    if (!module || !function || !count) {
        return {};
    }
    SymbolIdentity result{*module, *function, *count};
    return result;
}
} // namespace erlang_aot::semantic
