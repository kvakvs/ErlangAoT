#include "floats.hpp"
#include <cmath>

namespace erlang_aot::runtime::detail {
TermResult<double> float_operation(abi::v1::ImmediateOperation operation, double left, double right) {
    using Op = abi::v1::ImmediateOperation;
    switch (operation) {
    case Op::add:
        return left + right;
    case Op::subtract:
        return left - right;
    case Op::multiply:
        return left * right;
    case Op::divide:
        if (right == 0) {
            return std::unexpected(TermError::invalid_argument);
        }
        return left / right;
    case Op::positive:
        return left + 0.0;
    case Op::negative:
        return -left;
    case Op::absolute:
        return std::abs(left);
    default:
        return std::unexpected(TermError::wrong_type);
    }
}
} // namespace erlang_aot::runtime::detail
