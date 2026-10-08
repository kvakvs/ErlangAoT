#include "text.hpp"
#include <array>

namespace clause::runtime::builtins {
namespace {
// Digits of integer_digits, uppercase like OTP's integer_to_list/2.
constexpr std::string_view DIGITS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// The byte count of a UTF-8 sequence from its lead byte; 0 for a byte that cannot start one.
std::size_t sequence_length(unsigned char lead) {
    if (lead < 0x80) {
        return 1;
    }
    if (lead < 0xC2) {
        return 0;
    }
    return lead < 0xE0 ? 2 : (lead < 0xF0 ? 3 : (lead < 0xF5 ? 4 : 0));
}

// Decode one sequence of `length` bytes starting at `bytes[0]`; none for a bad continuation or overlong form.
std::optional<char32_t> decode_one(std::span<const std::byte> bytes, std::size_t length) {
    static constexpr std::array<char32_t, 5> MINIMUM{0, 0, 0x80, 0x800, 0x10000};
    const auto lead = std::to_integer<unsigned>(bytes[0]);
    char32_t code = length == 1 ? lead : lead & (0x7FU >> length);
    for (const auto byte : bytes.subspan(1, length - 1)) {
        const auto value = std::to_integer<unsigned>(byte);
        if ((value & 0xC0U) != 0x80U) {
            return std::nullopt;
        }
        code = (code << 6) | (value & 0x3FU);
    }
    const bool valid = length == 1 || (code >= MINIMUM.at(length) && unicode_character(code));
    return valid ? std::optional{code} : std::nullopt;
}

// The largest power of `base` used as a chunk of digits, and its digit count.
std::pair<std::uint64_t, unsigned> chunk(unsigned base) {
    std::uint64_t power = base;
    unsigned digits = 1;
    while (power <= (std::uint64_t{1} << 63) / base) {
        power *= base;
        ++digits;
    }
    return {power, digits};
}
} // namespace

std::u32string code_points(std::string_view text) {
    const auto bytes = std::as_bytes(std::span(text.data(), text.size()));
    return decode_utf8(bytes).value_or(std::u32string{});
}

std::optional<char32_t> next_utf8(std::span<const std::byte> bytes, std::size_t &at) {
    const auto length = sequence_length(std::to_integer<unsigned char>(bytes[at]));
    const auto code = length != 0 && length <= bytes.size() - at ? decode_one(bytes.subspan(at), length) : std::nullopt;
    at += code ? length : 0;
    return code;
}

std::optional<std::u32string> decode_utf8(std::span<const std::byte> bytes) {
    std::u32string result;
    result.reserve(bytes.size());
    for (std::size_t at = 0; at < bytes.size();) {
        const auto code = next_utf8(bytes, at);
        if (!code) {
            return std::nullopt;
        }
        result.push_back(*code);
    }
    return result;
}

void encode(char32_t code, std::string &text) {
    if (code < 0x80) {
        text.push_back(static_cast<char>(code));
        return;
    }
    const std::size_t length = code < 0x800 ? 2 : code < 0x10000 ? 3 : 4;
    static constexpr std::array<unsigned, 5> LEADS{0, 0, 0xC0, 0xE0, 0xF0};
    text.push_back(static_cast<char>(LEADS.at(length) | (code >> (6 * (length - 1)))));
    for (std::size_t i = length - 1; i > 0; --i) {
        text.push_back(static_cast<char>(0x80U | ((code >> (6 * (i - 1))) & 0x3FU)));
    }
}

std::string utf8(std::u32string_view codes) {
    std::string text;
    text.reserve(codes.size());
    for (const auto code : codes) {
        encode(code, text);
    }
    return text;
}

std::string integer_digits(const detail::Integer &value, unsigned base) {
    if (base == 10) {
        return detail::integer_text(value);
    }
    const auto [power, width] = chunk(base);
    detail::Integer magnitude = boost::multiprecision::abs(value);
    std::string reversed;
    for (;;) {
        detail::Integer quotient;
        detail::Integer remainder;
        boost::multiprecision::divide_qr(magnitude, detail::Integer(power), quotient, remainder);
        auto part = remainder.convert_to<std::uint64_t>();
        for (unsigned i = 0; i < width && (quotient != 0 || part != 0 || i == 0); ++i) {
            reversed.push_back(DIGITS[part % base]);
            part /= base;
        }
        if (quotient == 0) {
            break;
        }
        magnitude.swap(quotient);
    }
    if (value < 0) {
        reversed.push_back('-');
    }
    return {reversed.rbegin(), reversed.rend()};
}
} // namespace clause::runtime::builtins
