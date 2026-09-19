#pragma once
#include "discovery.hpp"

namespace erlang_aot::project {
// Assemble ordered translation units and deduplicate physical aliases within one target.
std::vector<std::filesystem::path> target_sources(const std::filesystem::path &base, const Target &target,
                                                  DiscoveryLimits limits = {});
} // namespace erlang_aot::project
