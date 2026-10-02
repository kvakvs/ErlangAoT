#pragma once
#include "v1.hpp"

namespace erlang_aot::abi::v1 {
// Guard argument errors are selection outcomes; infrastructure failures use the checked channel.
enum class ValueOutcome : std::uint8_t { success, bad_argument, failure };
enum class ImmediateOperation : std::uint8_t {
    exact_equal,
    exact_not_equal,
    equal,
    not_equal,
    less,
    less_equal,
    greater,
    greater_equal,
    is_atom,
    is_integer,
    is_number,
    is_boolean,
    is_tuple,
    is_list,
    is_binary,
    is_bitstring,
    is_float,
    is_map,
    is_pid,
    is_port,
    is_reference,
    is_function,
    is_function_arity,
    tuple_size,
    length,
    size,
    element,
    hd,
    tl,
    minimum,
    maximum
};
} // namespace erlang_aot::abi::v1

// A success-only output word separates semantic rejection from ownership/resource/internal failures.
std::uint8_t erlang_aot_immediate_v1(void *context, std::uint8_t operation, erlang_aot::abi::v1::TermWord left,
                                     erlang_aot::abi::v1::TermWord right,
                                     erlang_aot::abi::v1::TermWord *output) noexcept;
