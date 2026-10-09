#pragma once
#include <string_view>

namespace clause::semantic::types {
// Whether canonical decimal integer `left` is less than `right`, exactly, also beyond a host word.
inline bool decimal_less(std::string_view left, std::string_view right) {
    const bool negative = left.starts_with('-');
    if (negative != right.starts_with('-')) {
        return negative;
    }
    if (negative) {
        left.remove_prefix(1);
        right.remove_prefix(1);
    }
    const auto order = left.size() == right.size() ? left.compare(right) : (left.size() < right.size() ? -1 : 1);
    return negative ? order > 0 : order < 0;
}
} // namespace clause::semantic::types
