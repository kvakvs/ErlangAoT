#pragma once
#include "model.hpp"
#include <string_view>

namespace erlang_aot::project {
struct GlobLimits {
    // Bound matching transitions, including nested component comparisons.
    std::size_t work = 1000000;
};

struct Glob {
    // Preserve decoded path components and the originating manifest location.
    std::vector<std::u32string> components;
    Site site;
};

// Recognize wildcard or unsupported pattern syntax before literal source lookup.
bool is_pattern(std::string_view text);
// Decode portable slash-separated wildcard syntax without touching the filesystem.
Glob parse_glob(const Text &pattern);
// Match one relative generic UTF-8 path with a bounded iterative state table.
bool matches(const Glob &glob, std::string_view path, GlobLimits limits = {});
// Share a matching budget across every file considered by one source expansion.
bool matches_with_budget(const Glob &glob, std::string_view path, std::size_t &work);
} // namespace erlang_aot::project
