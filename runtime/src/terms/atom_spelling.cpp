#include <clause/runtime/atoms.hpp>

namespace clause::runtime {
namespace {
// Decode continuation bytes with explicit bounds and reject noncanonical scalar encodings.
bool continuation(std::string_view text, std::size_t &offset, unsigned length, std::uint32_t value) noexcept {
    static constexpr std::array<std::uint32_t, 5> minimum{0, 0, 0x80, 0x800, 0x10000};
    if (length == 0 || text.size() - offset < length - 1) {
        return false;
    }
    for (unsigned index = 1; index < length; ++index) {
        const auto byte = static_cast<unsigned char>(text[offset++]);
        if ((byte & 0xc0U) != 0x80U) {
            return false;
        }
        value = (value << 6) | (byte & 0x3fU);
    }
    return value >= minimum[length] && value <= 0x10ffff && !(value >= 0xd800 && value <= 0xdfff);
}

// Consume exactly one UTF-8 scalar, rejecting stray continuation and obsolete lead bytes.
bool scalar(std::string_view text, std::size_t &offset) noexcept {
    const auto byte = static_cast<unsigned char>(text[offset++]);
    if (byte < 0x80) {
        return true;
    }
    if (byte >= 0xc2 && byte <= 0xdf) {
        return continuation(text, offset, 2, byte & 0x1fU);
    }
    if (byte >= 0xe0 && byte <= 0xef) {
        return continuation(text, offset, 3, byte & 0xfU);
    }
    return byte >= 0xf0 && byte <= 0xf4 && continuation(text, offset, 4, byte & 7U);
}
} // namespace

bool valid_atom_spelling(std::string_view spelling) noexcept {
    if (spelling.size() > std::size_t{255} * 4) {
        return false;
    }
    std::size_t offset = 0;
    unsigned count = 0;
    while (offset < spelling.size()) {
        if (++count > 255 || !scalar(spelling, offset)) {
            return false;
        }
    }
    return true;
}
} // namespace clause::runtime
