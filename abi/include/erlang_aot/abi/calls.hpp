#pragma once
#include "v1.hpp"

namespace erlang_aot::abi::v1 {
// Selection failures stay local control flow; typed reasons identify the admitted Erlang error outcomes.
enum class ErrorReason : std::uint8_t {
    function_clause = 1,
    badmatch = 2,
    badarg = 3,
    badarg_value = 4,
    badarith = 5,
    badmap = 6,
    badkey = 7,
    badrecord = 8,
    case_clause = 9
};
} // namespace erlang_aot::abi::v1

// Native C++ services borrow a live context; generated code never inspects its layout.
std::uint8_t erlang_aot_call_failed_v2(void *context) noexcept;
// Preserve the first error; payload reasons (badmatch, case_clause, ...) retain ownership before root cleanup.
std::uint8_t erlang_aot_raise_v2(void *context, erlang_aot::abi::v1::ErrorReason reason,
                                 erlang_aot::abi::v1::TermWord value) noexcept;
