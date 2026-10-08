#include "implementation_debug.hpp"
#include <charconv>
#include <string_view>

namespace clause::cli {
namespace {
// Accept one signed decimal int32 with complete consumption and checked overflow.
std::optional<std::int32_t> step_number(std::string_view text) {
    if (text.starts_with('+')) {
        text.remove_prefix(1);
        if (text.starts_with('-')) {
            return {};
        }
    }
    if (text.empty()) {
        return {};
    }
    std::int32_t step = 0;
    const auto converted = std::from_chars(text.data(), text.data() + text.size(), step);
    if (converted.ec != std::errc{} || converted.ptr != text.data() + text.size()) {
        return {};
    }
    return step;
}

// Reject empty members and malformed suffixes rather than silently accepting a partial selection.
bool add_steps(std::string_view operand, ImplementationDebug &selection) {
    while (true) {
        const auto comma = operand.find(',');
        const auto step = step_number(operand.substr(0, comma));
        if (!step) {
            return false;
        }
        selection.enable(*step);
        if (comma == std::string_view::npos) {
            return true;
        }
        operand.remove_prefix(comma + 1);
    }
}
} // namespace

std::optional<std::string> parse_implementation_debug(std::span<char *> &remaining, ImplementationDebug &selection) {
    if (remaining.empty() || std::string_view(remaining.front()).empty()) {
        return "expected an integer or comma-separated list after --impldebug";
    }
    auto updated = selection;
    if (!add_steps(remaining.front(), updated)) {
        return "--impldebug expects decimal integers in -2147483648..2147483647, separated by commas without spaces";
    }
    selection = std::move(updated);
    remaining = remaining.subspan(1);
    return {};
}
} // namespace clause::cli
