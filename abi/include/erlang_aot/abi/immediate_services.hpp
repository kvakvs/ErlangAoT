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
    maximum,
    logical_not,
    logical_and,
    logical_or,
    logical_xor,
    boolean_check,
    add,
    subtract,
    multiply,
    integer_divide,
    remainder,
    bit_and,
    bit_or,
    bit_xor,
    shift_left,
    shift_right,
    positive,
    negative,
    bit_not,
    absolute,
    divide,
    to_float,
    round,
    trunc,
    floor,
    ceil,
    map_size,
    map_get,
    is_map_key,
    bit_size,
    byte_size,
    binary_part,
    // Compound compiler lowering composes checked tuple/numeric services for record tests.
    is_record,
    // Compound compiler lowering validates integer bounds before comparing the candidate.
    is_integer_range,
    // Body-only erlang:display/1 lowers to erlang_aot_display_v1, never to the immediate service.
    display
};
} // namespace erlang_aot::abi::v1

// A success-only output word separates semantic rejection from ownership/resource/internal failures.
std::uint8_t erlang_aot_immediate_v1(void *context, std::uint8_t operation, erlang_aot::abi::v1::TermWord left,
                                     erlang_aot::abi::v1::TermWord right,
                                     erlang_aot::abi::v1::TermWord *output) noexcept;
