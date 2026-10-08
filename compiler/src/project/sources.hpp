#pragma once
#include "discovery.hpp"

namespace clause::project {
// Assemble ordered translation units and deduplicate physical aliases within one target.
std::vector<std::filesystem::path> target_sources(const std::filesystem::path &base, const Target &target,
                                                  DiscoveryLimits limits = {});
} // namespace clause::project
