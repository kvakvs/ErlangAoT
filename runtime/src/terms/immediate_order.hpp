#pragma once
#include <erlang_aot/runtime/terms.hpp>

namespace erlang_aot::runtime::detail {
// Numeric and term ordering dispatch only validated admitted representations; atom IDs never define order.
TermResult<int> immediate_order(const Term &left, const Term &right) noexcept;
} // namespace erlang_aot::runtime::detail
