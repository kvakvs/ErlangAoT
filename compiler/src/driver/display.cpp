#include "display.hpp"

namespace clause::cli {
std::string quote_text(const std::string_view text) {
    constexpr std::string_view hex = "0123456789abcdef";
    std::string result;
    result.reserve(text.size() + 2);
    result += '"';
    for (const unsigned char character : text) {
        if (character < 32 || character == 127 || character == '"' || character == '\\') {
            result += "\\x";
            result += hex[character >> 4];
            result += hex[character & 15];
        } else {
            result += static_cast<char>(character);
        }
    }
    return result + '"';
}
} // namespace clause::cli
