#pragma once
#include <erlang_aot/runtime/terms.hpp>

namespace erlang_aot::runtime::detail {
// Compare admitted immutable graphs iteratively; exact mode retains atom identity across host runtimes.
TermResult<int> structural_order(const Term &left, const Term &right, bool exact = false) noexcept;
// Share a total traversal budget across staged map key comparisons and immutable lookups.
TermResult<int> structural_order(const Term &left, const Term &right, bool exact, std::size_t &remaining) noexcept;
} // namespace erlang_aot::runtime::detail
