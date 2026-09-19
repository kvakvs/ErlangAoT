#pragma once
#include "glob.hpp"
#include "model.hpp"

namespace erlang_aot::project {
struct DiscoveryLimits {
    // Bound visited directory entries, recursion depth, and total matching work per expansion.
    std::size_t entries = 100000;
    std::size_t depth = 128;
    std::size_t work = 16000000;
};

// Expand one wildcard selection deterministically, rejecting an unmatched pattern.
std::vector<std::filesystem::path> wildcard_sources(const std::filesystem::path &base, const Text &pattern,
                                                    DiscoveryLimits limits = {});
// Recursively collect regular Erlang sources under one explicitly named directory.
std::vector<std::filesystem::path> directory_sources(const std::filesystem::path &base, const Text &directory,
                                                     DiscoveryLimits limits = {});
} // namespace erlang_aot::project
