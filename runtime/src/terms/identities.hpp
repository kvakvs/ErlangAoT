#pragma once
#include "term_text.hpp"
#include <cstdint>
#include <erlang_aot/runtime/terms.hpp>

// Pids and references (docs/terms.md#pids-and-references): an immediate pid word carries its process number; a
// reference cell carries a number unique within the program run.
namespace erlang_aot::runtime::detail {
// The number of a reference; wrong_type for any other term.
TermResult<std::uint64_t> reference_number(const Term &value) noexcept;
// Order two pids or two references: by process number or reference number, as OTP orders local identities.
TermResult<int> identity_order(const Term &left, const Term &right) noexcept;
// Print a pid as <0.Number.Serial> and a reference as #Ref<0.N2.N1.N0>, as OTP prints local identities.
TermResult<void> print_identity(const Term &value, TextOutput &out);
} // namespace erlang_aot::runtime::detail
