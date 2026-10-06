#pragma once
#include <erlang_aot/runtime/terms.hpp>

namespace erlang_aot::runtime::detail {
// Compare admitted immutable graphs iteratively with no work cap, as in OTP; exact mode retains atom identity across
// host runtimes.
TermResult<int> structural_order(const Term &left, const Term &right, bool exact = false) noexcept;
} // namespace erlang_aot::runtime::detail
