#pragma once
#include <boost/multiprecision/cpp_int.hpp>
#include <erlang_aot/abi/immediate_services.hpp>
#include <erlang_aot/runtime/terms.hpp>

namespace erlang_aot::runtime::detail {
// Eager owned intermediates avoid expression-template references surviving a checked-result boundary.
using Integer = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<>, boost::multiprecision::et_off>;
// Bound stored magnitude and arithmetic work separately from Erlang operand errors.
inline constexpr std::size_t integer_bit_limit = 1'000'000;
inline constexpr std::size_t integer_decimal_limit = 10'000;

// Read canonical signed-magnitude limb extent without allocating an absolute-value temporary.
std::size_t integer_bits(const Integer &value) noexcept;
// Admit checked integers only; temporary multiprecision values never borrow mutable heap storage.
TermResult<Integer> integer_read(const Term &value);
// Parse bounded decimal syntax, accepting an optional sign and normalizing through construction.
TermResult<Integer> integer_parse(std::string_view text);
// Serialize an owned integer independently of host Term/error transport.
std::string integer_text(const Integer &value);
// Accumulate validated decimal digits independently of optional sign and checked host admission.
Integer integer_digits(std::string_view digits);
// Import bounded canonical word magnitudes, including empty/zero input, without retaining the source buffer.
Integer integer_words(std::span<const Word> words, bool negative);
// Combine canonical magnitudes using explicit unsigned carry/borrow and normalize the result sign.
Integer integer_sum(const Integer &left, const Integer &right, bool subtract);
// Operate on owned multiprecision inputs independently of runtime Term and checked-call transport.
TermResult<Integer> integer_binary(abi::v1::ImmediateOperation operation, Integer left, const Integer &right);
TermResult<Integer> integer_unary(abi::v1::ImmediateOperation operation, const Integer &value);
// Share exact integer operations between authorized body/guard services and promotion fallbacks.
TermResult<Term> integer_service(ProcessContext &context, abi::v1::ImmediateOperation operation, const Term &left,
                                 const Term &right);

struct IntegerAccess {
    // Normalize small values and publish larger immutable sign/magnitude words transactionally.
    static TermResult<Term> make(ProcessHeap &heap, const Integer &value);
};
} // namespace erlang_aot::runtime::detail
