#pragma once
#include <bit>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace test_records {
// Share unambiguous byte encoding across scanner, preprocessing, and AST test dumps.
inline std::string hex(std::string_view bytes) {
    if (bytes.empty()) {
        return "-";
    }
    std::ostringstream output;
    for (const unsigned char byte : bytes) {
        output << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte);
    }
    return output.str();
}

// Record exact binary64 payloads instead of relying on decimal formatting choices.
inline std::string float_bits(double value) {
    std::ostringstream output;
    output << std::hex << std::setw(16) << std::setfill('0') << std::bit_cast<std::uint64_t>(value);
    return output.str();
}
} // namespace test_records
