#pragma once
#include "integers.hpp"

namespace clause::runtime::detail {
// Convert finite binary64 values exactly to truncated integers, without host-width narrowing.
Integer float_integer(double value);
// Round arbitrary integers to nearest binary64, ties to even; overflow is a semantic range error.
TermResult<double> integer_float(const Integer &value);
// Coerce numeric operands only after checking their owned representation.
TermResult<double> number_float(const Term &value);
// Compare numbers without rounding arbitrary integers; exact mode distinguishes representation and signed zero.
TermResult<int> numeric_order(const Term &left, const Term &right, bool exact);
// Share finite arithmetic and numeric conversion between body and guard dispatch.
TermResult<Term> numeric_service(ProcessContext &context, abi::v1::ImmediateOperation operation, const Term &left,
                                 const Term &right);
TermResult<double> float_operation(abi::v1::ImmediateOperation operation, double left, double right);

struct FloatAccess {
    // Publish finite binary64 bits into immutable checked heap storage.
    static TermResult<Term> make(ProcessHeap &heap, double value);
};
} // namespace clause::runtime::detail
