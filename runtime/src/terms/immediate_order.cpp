#include "immediate_order.hpp"
#include "structural_order.hpp"

namespace clause::runtime::detail {
TermResult<int> immediate_order(const Term &left, const Term &right) noexcept { return structural_order(left, right); }
} // namespace clause::runtime::detail
