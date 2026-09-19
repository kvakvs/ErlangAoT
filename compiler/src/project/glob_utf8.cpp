#include "glob_utf8.hpp"
#include "diagnostics.hpp"
#include <array>

namespace erlang_aot::project {
namespace {
// Determine the canonical UTF-8 sequence width from its leading byte.
std::size_t width(unsigned char byte, const Site &site) {
    if (byte < 0x80) {
        return 1;
    }
    if (byte >= 0xc2 && byte <= 0xdf) {
        return 2;
    }
    if (byte >= 0xe0 && byte <= 0xef) {
        return 3;
    }
    if (byte >= 0xf0 && byte <= 0xf4) {
        return 4;
    }
    fail(site, "unsupported filename encoding: invalid UTF-8");
}

// Consume one canonical Unicode scalar while checking continuation bytes.
char32_t scalar(std::string_view text, std::size_t &position, const Site &site) {
    const auto first = static_cast<unsigned char>(text[position]);
    const auto count = width(first, site);
    if (count > text.size() - position) {
        fail(site, "unsupported filename encoding: truncated UTF-8");
    }
    constexpr std::array<unsigned char, 5> masks{0, 0x7f, 0x1f, 0x0f, 0x07};
    char32_t result = first & masks[count];
    for (std::size_t i = 1; i < count; ++i) {
        const auto byte = static_cast<unsigned char>(text[position + i]);
        if ((byte & 0xc0) != 0x80) {
            fail(site, "unsupported filename encoding: invalid UTF-8 continuation");
        }
        result = (result << 6) | (byte & 0x3f);
    }
    constexpr std::array<char32_t, 5> minima{0, 0, 0x80, 0x800, 0x10000};
    if (result < minima[count] || result > 0x10ffff || (result >= 0xd800 && result <= 0xdfff)) {
        fail(site, "unsupported filename encoding: noncanonical UTF-8");
    }
    position += count;
    return result;
}
} // namespace

std::u32string filename_scalars(std::string_view text, const Site &site) {
    std::u32string result;
    std::size_t position = 0;
    while (position < text.size()) {
        result.push_back(scalar(text, position, site));
    }
    return result;
}
} // namespace erlang_aot::project
