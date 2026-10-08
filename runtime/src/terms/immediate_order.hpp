#pragma once
#include <clause/runtime/terms.hpp>

namespace clause::runtime::detail {
// Numeric and term ordering dispatch only validated admitted representations; atom IDs never define order.
TermResult<int> immediate_order(const Term &left, const Term &right) noexcept;
} // namespace clause::runtime::detail
