#include "integers.hpp"
#include <clause/abi/immediate_services.hpp>
#include <clause/runtime/process_context.hpp>

namespace clause::runtime::detail {
namespace {
using Op = abi::v1::ImmediateOperation;

// Bound the eventual shifted result before allocating; negative counts reverse the direction.
TermResult<Integer> shift(const Integer &value, const Integer &count) {
    if (value == 0 || count == 0) {
        return value;
    }
    const auto bits = integer_bits(value);
    if (count < 0) {
        if (-count >= bits) {
            return Integer(value < 0 ? -1 : 0);
        }
        return Integer(value >> (-count).convert_to<std::size_t>());
    }
    if (count > integer_bit_limit - bits) {
        return std::unexpected(TermError::system_limit);
    }
    return Integer(value << count.convert_to<std::size_t>());
}

// cpp_int supplies exact signed infinite-two's-complement bitwise operations on normalized values.
TermResult<Integer> bitwise(Op operation, const Integer &left, const Integer &right) {
    switch (operation) {
    case Op::bit_and:
        return Integer(left & right);
    case Op::bit_or:
        return Integer(left | right);
    case Op::bit_xor:
        return Integer(left ^ right);
    case Op::shift_left:
        return shift(left, right);
    case Op::shift_right:
        return shift(left, -right);
    default:
        return std::unexpected(TermError::invalid_argument);
    }
}

// Division truncates toward zero and remainder carries the dividend sign, without native integer UB.
TermResult<Integer> division(Op operation, const Integer &left, const Integer &right) {
    if (right == 0) {
        return std::unexpected(TermError::invalid_argument);
    }
    if (operation == Op::integer_divide) {
        return Integer(left / right);
    }
    return Integer(left % right);
}

} // namespace

// Result growth is bounded before multiplication; addition/subtraction need at most one transient extra bit.
TermResult<Integer> integer_binary(Op operation, Integer left, const Integer &right) {
    switch (operation) {
    case Op::add:
        return integer_sum(left, right, false);
    case Op::subtract:
        return integer_sum(left, right, true);
    case Op::multiply:
        // The product has at least bits(left) + bits(right) - 1 bits.
        if (left != 0 && right != 0 && integer_bits(left) + integer_bits(right) > integer_bit_limit + 1) {
            return std::unexpected(TermError::system_limit);
        }
        left *= right;
        return left;
    case Op::integer_divide:
    case Op::remainder:
        return division(operation, left, right);
    default:
        return bitwise(operation, left, right);
    }
}

// Keep unary sign/absolute/complement semantics independent of binary placeholder admission.
TermResult<Integer> integer_unary(Op operation, const Integer &value) {
    switch (operation) {
    case Op::positive:
        return value;
    case Op::negative:
        return Integer(-value);
    case Op::bit_not:
        return Integer(-value - 1);
    case Op::absolute:
        return Integer(value < 0 ? -value : value);
    default:
        return std::unexpected(TermError::invalid_argument);
    }
}

} // namespace clause::runtime::detail
