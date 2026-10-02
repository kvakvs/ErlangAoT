#include "floats.hpp"
#include <cmath>
#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime::detail {
namespace {
using Op = abi::v1::ImmediateOperation;

// Conversion BIFs preserve integer operands except float/1, and round ties away from zero.
TermResult<Term> convert(ProcessContext &context, Op operation, const Term &value) {
    if (operation == Op::to_float) {
        if (value.is_float()) {
            return value;
        }
        return number_float(value).and_then([&](double real) { return FloatAccess::make(context.heap(), real); });
    }
    if (value.is_integer()) {
        return value;
    }
    const auto real = value.float_value();
    if (!real) {
        return std::unexpected(real.error());
    }
    double integral = std::trunc(*real);
    if (operation == Op::round) {
        integral = std::round(*real);
    }
    if (operation == Op::floor) {
        integral = std::floor(*real);
    }
    if (operation == Op::ceil) {
        integral = std::ceil(*real);
    }
    return IntegerAccess::make(context.heap(), float_integer(integral));
}

// Integer-only operators reject floats instead of coercing operands.
bool integral(Op operation) {
    return (operation >= Op::integer_divide && operation <= Op::shift_right) || operation == Op::bit_not;
}
} // namespace

TermResult<Term> numeric_service(ProcessContext &context, Op operation, const Term &left, const Term &right) {
    if (operation >= Op::to_float) {
        return convert(context, operation, left);
    }
    if (integral(operation) || (operation != Op::divide && !left.is_float() && !right.is_float())) {
        return integer_service(context, operation, left, right);
    }
    const auto lhs = number_float(left);
    if (!lhs) {
        return std::unexpected(lhs.error());
    }
    const bool binary = operation < Op::positive || operation == Op::divide;
    const auto rhs = binary ? number_float(right) : TermResult<double>{0};
    if (!rhs) {
        return std::unexpected(rhs.error());
    }
    return float_operation(operation, *lhs, *rhs).and_then([&](double value) {
        return FloatAccess::make(context.heap(), value);
    });
}
} // namespace erlang_aot::runtime::detail
