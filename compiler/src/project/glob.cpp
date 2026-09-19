#include "glob.hpp"
#include "diagnostics.hpp"
#include "glob_utf8.hpp"
#include <algorithm>
#include <cstdint>

namespace erlang_aot::project {
namespace {
struct ComponentPattern {
    // Distinguish wildcard syntax from filename text at the matching boundary.
    const std::u32string &value;
};

// Split generic paths on separators, preserving Unicode scalar boundaries.
std::vector<std::u32string> components(std::string_view path, const Site &site) {
    const auto decoded = filename_scalars(path, site);
    std::vector<std::u32string> result;
    std::size_t start = 0;
    while (start < decoded.size()) {
        const auto end = decoded.find(U'/', start);
        const auto count = end == std::u32string::npos ? decoded.size() - start : end - start;
        if (count != 0) {
            result.push_back(decoded.substr(start, count));
        }
        start += count + 1;
    }
    return result;
}

// Charge every state transition against one per-match work budget.
void charge(std::size_t &work, const Site &site) {
    if (work == 0) {
        fail(site, "wildcard work limit exceeded");
    }
    --work;
}

// Compute one character-pattern row with linear space and no recursive backtracking.
std::vector<std::uint8_t> character_row(char32_t token, const std::u32string &text,
                                        const std::vector<std::uint8_t> &previous, std::size_t &work,
                                        const Site &site) {
    std::vector<std::uint8_t> current(text.size() + 1, 0);
    current[0] = static_cast<std::uint8_t>(token == U'*' && previous[0]);
    for (std::size_t i = 1; i <= text.size(); ++i) {
        charge(work, site);
        if (token == U'*') {
            current[i] = static_cast<std::uint8_t>(previous[i] || current[i - 1]);
        } else {
            current[i] = static_cast<std::uint8_t>(previous[i - 1] && (token == U'?' || token == text[i - 1]));
        }
    }
    return current;
}

// Match a single component; wildcards cannot consume directory separators.
bool component_match(ComponentPattern pattern, const std::u32string &text, std::size_t &work, const Site &site) {
    std::vector<std::uint8_t> row(text.size() + 1, 0);
    row[0] = 1;
    for (const auto token : pattern.value) {
        charge(work, site);
        row = character_row(token, text, row, work, site);
    }
    return row.back() != 0;
}

// Compute a path-pattern row; only a whole ** component spans directories.
std::vector<std::uint8_t> path_row(const std::u32string &pattern, const std::vector<std::u32string> &text,
                                   const std::vector<std::uint8_t> &previous, std::size_t &work, const Site &site) {
    std::vector<std::uint8_t> current(text.size() + 1, 0);
    current[0] = static_cast<std::uint8_t>(pattern == U"**" && previous[0]);
    for (std::size_t i = 1; i <= text.size(); ++i) {
        charge(work, site);
        if (pattern == U"**") {
            current[i] = static_cast<std::uint8_t>(previous[i] || current[i - 1]);
        } else if (previous[i - 1]) {
            current[i] = static_cast<std::uint8_t>(component_match({pattern}, text[i - 1], work, site));
        }
    }
    return current;
}
} // namespace

bool is_pattern(std::string_view text) { return text.find_first_of("*?[]{}!") != std::string_view::npos; }

Glob parse_glob(const Text &pattern) {
    if (pattern.value.find_first_of("[]{}!\\") != std::string::npos) {
        fail(pattern.site, "unsupported wildcard construct");
    }
    Glob result{components(pattern.value, pattern.site), pattern.site};
    for (const auto &component : result.components) {
        if (component != U"**" && component.find(U"**") != std::u32string::npos) {
            fail(pattern.site, "** must be an entire path component");
        }
    }
    return result;
}

bool matches(const Glob &glob, std::string_view path, GlobLimits limits) {
    const auto text = components(path, glob.site);
    std::vector<std::uint8_t> row(text.size() + 1, 0);
    row[0] = 1;
    for (const auto &component : glob.components) {
        charge(limits.work, glob.site);
        row = path_row(component, text, row, limits.work, glob.site);
    }
    return row.back() != 0;
}
} // namespace erlang_aot::project
