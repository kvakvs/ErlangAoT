#pragma once
#include "term_text.hpp"
#include <clause/runtime/terms.hpp>
#include <cstdint>

// Pids, ports and references (docs/terms.md#pids-and-references, docs/ports.md#identity): an immediate pid or port
// word carries its number; a reference cell carries a number unique within the program run.
namespace clause::runtime::detail {
// The number of a reference; wrong_type for any other term.
TermResult<std::uint64_t> reference_number(const Term &value) noexcept;
// Order two pids, two ports or two references by number, as OTP orders local identities.
TermResult<int> identity_order(const Term &left, const Term &right) noexcept;
// Print a pid as <0.Number.Serial>, a port as #Port<0.Number> and a reference as #Ref<0.N2.N1.N0>, as OTP prints
// local identities.
TermResult<void> print_identity(const Term &value, TextOutput &out);
} // namespace clause::runtime::detail
