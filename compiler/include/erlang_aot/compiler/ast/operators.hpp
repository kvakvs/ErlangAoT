#pragma once
#include <cstdint>

namespace erlang_aot::ast {
// Closed syntax identities; evaluation and operand legality belong to later passes.
enum class UnaryOperator : std::uint8_t { positive, negative, bit_not, logical_not };
enum class BinaryOperator : std::uint8_t {
    send,
    or_else,
    and_also,
    equal,
    not_equal,
    less_equal,
    less,
    greater_equal,
    greater,
    exact_equal,
    exact_not_equal,
    append,
    subtract_list,
    add,
    subtract,
    bit_or,
    bit_xor,
    shift_left,
    shift_right,
    logical_or,
    logical_xor,
    multiply,
    divide,
    integer_divide,
    remainder,
    bit_and,
    logical_and
};
} // namespace erlang_aot::ast
