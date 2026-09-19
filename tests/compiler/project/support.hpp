#pragma once
#include <source_location>
#include <stdexcept>
#include <string>

// Retain assertions in release builds and report the failing test line.
inline void require(bool condition, const std::source_location &where = std::source_location::current()) {
    if (!condition) {
        throw std::runtime_error(std::string(where.file_name()) + ":" + std::to_string(where.line()) +
                                 ": assertion failed");
    }
}
