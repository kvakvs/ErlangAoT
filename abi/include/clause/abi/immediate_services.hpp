#pragma once
#include "v1.hpp"

namespace clause::abi::v1 {
// Guard argument errors are selection outcomes; infrastructure failures use the checked channel. An integer result
// beyond the ERTS size limit rejects a guard and raises error:system_limit in a body.
enum class ValueOutcome : std::uint8_t { success, bad_argument, failure, system_limit };
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
    // Native is_record/1: whether the value is a native record.
    is_native_record,
    is_function_arity,
    tuple_size,
    length,
    size,
    element,
    hd,
    tl,
    minimum,
    maximum,
    // self/0 and node/0 take no operand; node/1 takes a pid, reference or port (else bad_argument).
    self,
    node,
    node_of,
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
    // Body-only erlang:display/1 lowers to CLAUSE_display_v1, never to the immediate service.
    display,
    // Body-only erlang:halt/0,1 lowers to CLAUSE_halt_v1 and never returns.
    halt,
    // Body-only erlang:error/1,2,3, exit/1 and throw/1 lower to CLAUSE_raise_v2 or CLAUSE_error_v1 and
    // never return; erlang:raise/3 lowers to CLAUSE_reraise_v2 and returns badarg for invalid arguments.
    raise
};
} // namespace clause::abi::v1

// A success-only output word separates semantic rejection from ownership/resource/internal failures.
std::uint8_t CLAUSE_immediate_v1(void *context, std::uint8_t operation, clause::abi::v1::TermWord left,
                                 clause::abi::v1::TermWord right, clause::abi::v1::TermWord *output) noexcept;
