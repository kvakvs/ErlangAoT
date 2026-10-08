#pragma once
#include "../terms/integers.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// Character and digit text shared by the conversion and io builtins.
namespace clause::runtime::builtins {
// Largest Unicode code point.
inline constexpr std::int64_t MAX_CODE_POINT = 0x10FFFF;

// Whether `code` is a Unicode scalar value: a code point that is not a surrogate.
constexpr bool unicode_character(std::int64_t code) {
    return code >= 0 && code <= MAX_CODE_POINT && (code < 0xD800 || code > 0xDFFF);
}

// The code points of valid UTF-8 text (atom spellings are validated when interned).
std::u32string code_points(std::string_view text);

// The UTF-8 character at `bytes[at]`, advancing `at` past it; none, leaving `at`, for an invalid sequence.
std::optional<char32_t> next_utf8(std::span<const std::byte> bytes, std::size_t &at);

// The code points of UTF-8 bytes; none when the bytes are not exactly a sequence of valid UTF-8 characters.
std::optional<std::u32string> decode_utf8(std::span<const std::byte> bytes);

// Append `code` (a Unicode scalar value) to `text` as UTF-8.
void encode(char32_t code, std::string &text);

// The UTF-8 text of Unicode scalar values.
std::string utf8(std::u32string_view codes);

// The digits of `value` in `base` (2..36), uppercase, most significant first, with a leading '-' when negative.
std::string integer_digits(const detail::Integer &value, unsigned base);
} // namespace clause::runtime::builtins
