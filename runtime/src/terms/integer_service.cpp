#include "integers.hpp"
#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime::detail {
using Op = abi::v1::ImmediateOperation;

TermResult<Term> integer_service(ProcessContext &context, Op operation, const Term &left, const Term &right) {
    const auto lhs = integer_read(left);
    if (!lhs) {
        return std::unexpected(lhs.error());
    }
    TermResult<Integer> result;
    if (operation >= Op::positive) {
        result = integer_unary(operation, *lhs);
    } else {
        const auto rhs = integer_read(right);
        if (!rhs) {
            return std::unexpected(rhs.error());
        }
        result = integer_binary(operation, *lhs, *rhs);
    }
    return result.and_then([&](const auto &value) { return IntegerAccess::make(context.heap(), value); });
}
} // namespace erlang_aot::runtime::detail
