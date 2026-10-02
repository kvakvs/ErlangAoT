#include "immediate_order.hpp"
#include "structural_order.hpp"

namespace erlang_aot::runtime::detail {
TermResult<int> immediate_order(const Term &left, const Term &right) noexcept { return structural_order(left, right); }
} // namespace erlang_aot::runtime::detail
