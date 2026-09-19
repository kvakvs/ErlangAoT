#include "printable.hpp"
#include <array>
#include <charconv>

namespace erlang_aot::printing {
namespace {
// Match io_lib:printable_unicode_list/1's non-ASCII ranges, excluding surrogates and FFFE/FFFF.
bool printable_unicode(const char32_t value) {
    return (value >= 0xa0 && value < 0xd800) || (value > 0xdfff && value < 0xfffe) ||
           (value > 0xffff && value <= 0x10ffff);
}

// Keep ordinary ASCII fast and allow Erlang's escaped controls in otherwise printable strings.
bool printable_codepoint(const char32_t value) {
    if (value >= U' ' && value <= U'~') {
        return true;
    }
    // Precompute direct lookup flags for Erlang's escaped control characters.
    static constexpr auto controls = [] {
        std::array<bool, 0x1f> table{};
        table[U'\n'] = true;
        table[U'\r'] = true;
        table[U'\t'] = true;
        table[U'\v'] = true;
        table[U'\b'] = true;
        table[U'\f'] = true;
        table[0x1b] = true;
        return table;
    }();
    return (value < controls.size() && controls[value]) || printable_unicode(value);
}
} // namespace

std::optional<char32_t> printable_character(const Integer &value) {
    const auto &digits = value.decimal;
    unsigned number = 0;
    if (const auto [ptr, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), number);
        ec != std::errc{} || ptr != digits.data() + digits.size()) {
        return std::nullopt;
    }
    const auto character = static_cast<char32_t>(number);
    if (!printable_codepoint(character)) {
        return std::nullopt;
    }
    return character;
}
} // namespace erlang_aot::printing
