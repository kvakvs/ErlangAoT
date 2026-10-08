#pragma once
#include "model.hpp"
#include <span>

namespace clause::project {
// Select all targets by default or unique requested indices in selector order.
std::vector<std::size_t> select_targets(const Manifest &manifest, std::span<const std::string> selectors);
} // namespace clause::project
