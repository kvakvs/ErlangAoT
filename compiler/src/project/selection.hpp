#pragma once
#include "model.hpp"
#include <span>

namespace erlang_aot::project {
// Select all targets by default or unique requested indices in selector order.
std::vector<std::size_t> select_targets(const Manifest &manifest, std::span<const std::string> selectors);
} // namespace erlang_aot::project
