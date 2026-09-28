#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace erlang_aot {
class ImplementationDebug {
  public:
    // Enable one implementation step without duplicating an earlier selection.
    void enable(std::int32_t step) {
        const auto position = std::ranges::lower_bound(steps_, step);
        if (position == steps_.end() || *position != step) {
            steps_.insert(position, step);
        }
    }

    // Let each implementation phase gate its own optional diagnostic output.
    bool enabled(std::int32_t step) const { return std::ranges::binary_search(steps_, step); }

  private:
    // Keep sorted unique selections value-owned and allocation-free to move between requests.
    std::vector<std::int32_t> steps_;
};
} // namespace erlang_aot
