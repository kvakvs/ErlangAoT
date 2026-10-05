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
    case_clause = 9,
    if_clause = 10,
    // erlang:error/1,2,3, exit/1 and throw/1: the payload is the whole reason and the ID selects the class.
    raised_error = 11,
    raised_exit = 12,
    raised_throw = 13,
    // A try's `of` clauses did not match; the payload is the body value.
    try_clause = 14,
    // No `else` clause of a maybe matched; the payload is the unmatched value.
    else_clause = 15
};
} // namespace erlang_aot::abi::v1

// Native C++ services borrow a live context; generated code never inspects its layout.
std::uint8_t erlang_aot_call_failed_v2(void *context) noexcept;
// Preserve the first error; payload reasons (badmatch, case_clause, ...) retain ownership before root cleanup.
std::uint8_t erlang_aot_raise_v2(void *context, erlang_aot::abi::v1::ErrorReason reason,
                                 erlang_aot::abi::v1::TermWord value) noexcept;
// Turn the pending Erlang exception into the value of `catch Expr` in `output` and clear the channel;
// halts and infrastructure failures stay pending and return a nonzero status.
std::uint8_t erlang_aot_catch_v1(void *context, erlang_aot::abi::v1::TermWord *output) noexcept;
// Move the pending Erlang exception into its class atom, reason and stack trace terms (try ... catch) and clear
// the channel; halts and infrastructure failures stay pending and return a nonzero status.
std::uint8_t erlang_aot_exception_v2(void *context, erlang_aot::abi::v1::TermWord *exception_class,
                                     erlang_aot::abi::v1::TermWord *reason,
                                     erlang_aot::abi::v1::TermWord *stack) noexcept;
// Raise Class:Reason with a given stack trace (erlang:raise/3, or a try with no matching catch clause). An invalid
// class or malformed stack records nothing and returns a nonzero status: raise/3 then evaluates to badarg.
std::uint8_t erlang_aot_reraise_v2(void *context, erlang_aot::abi::v1::TermWord exception_class,
                                   erlang_aot::abi::v1::TermWord reason, erlang_aot::abi::v1::TermWord stack) noexcept;
// Raise erlang:error/2,3: a list `arguments` replaces the arity in the top stack frame, any other term is ignored.
std::uint8_t erlang_aot_error_v1(void *context, erlang_aot::abi::v1::TermWord reason,
                                 erlang_aot::abi::v1::TermWord arguments) noexcept;
