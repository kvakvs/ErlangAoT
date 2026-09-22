#pragma once

// REVIEW SKETCH ONLY: optional explicit codec utilities, independent of module registration and lookup.
// Container/list mappings apply only when native code explicitly requests encoding or decoding.
#include "callable.hpp"

#include <array>
#include <concepts>
#include <initializer_list>
#include <ranges>
#include <string>
#include <type_traits>

namespace erlang_aot::runtime {
// Bound explicit codec operations; function registration and invocation do not use these limits.
struct ConversionLimits final {
    // Count sequence elements across one explicit conversion.
    std::size_t max_elements = 4096;
    // Bound nesting before entering another list/container conversion.
    std::size_t max_depth = 32;
    // Limit total copied payload bytes with checked arithmetic.
    std::size_t max_bytes = std::size_t{1} * 1024 * 1024;
};

// Share accounting across one explicitly requested recursive codec operation.
class ConversionBudget final {
  public:
    // Begin a fresh bounded conversion with validated nonzero limits.
    explicit ConversionBudget(ConversionLimits limits);
    // Charge aggregate element and byte counts with checked, non-wrapping arithmetic.
    CallResult<void> charge(std::size_t elements, std::size_t bytes);
    // Enter/leave one nested container; implementations must balance entry on every failure path.
    CallResult<void> enter();
    void leave() noexcept;

  private:
    // Preserve caller-selected caps throughout argument and result conversion.
    ConversionLimits limits_;
    // Track aggregate work already consumed, including nested containers and strings.
    std::size_t elements_ = 0;
    std::size_t bytes_ = 0;
    // Track current recursive nesting rather than the number of previously visited containers.
    std::size_t depth_ = 0;
};

// Specialize explicitly for additional value types; unsupported types have no codec.
template <typename Value> struct NativeCodec;

// Admit owned native parameter types; the decoder is an explicit utility, never a dispatch operation.
template <typename Value>
concept NativeArgument = std::same_as<Value, std::remove_cvref_t<Value>> && std::movable<Value> &&
                         requires(ProcessContext &context, const Term &term, ConversionBudget &budget) {
                             { NativeCodec<Value>::decode(context, term, budget) } -> std::same_as<CallResult<Value>>;
                         };

// Require an owned result value with a checked encoder into the caller's heap.
template <typename Value>
concept NativeReturn = std::same_as<Value, std::remove_cvref_t<Value>> && std::movable<Value> &&
                       requires(ProcessContext &context, const Value &value, ConversionBudget &budget) {
                           { NativeCodec<Value>::encode(context, value, budget) } -> std::same_as<CallResult<Term>>;
                       };

// Support bounded integers explicitly, keeping booleans and character encodings separate.
template <typename Value>
concept NativeInteger = std::integral<Value> && sizeof(Value) <= sizeof(std::uint64_t) && !std::same_as<Value, bool> &&
                        !std::same_as<Value, char> && !std::same_as<Value, wchar_t> && !std::same_as<Value, char8_t> &&
                        !std::same_as<Value, char16_t> && !std::same_as<Value, char32_t>;

// Decode with full signed/unsigned range checks; encode uint64_t without narrowing to int64_t.
template <NativeInteger Value> struct NativeCodec<Value> {
    // Accept only integer terms whose exact mathematical value fits Value.
    static CallResult<Value> decode(ProcessContext &context, const Term &term, ConversionBudget &budget);
    // Construct an exact Erlang integer, using bignum storage when necessary.
    static CallResult<Term> encode(ProcessContext &context, Value value, ConversionBudget &budget);
};

// Provide explicit floating codecs for the native binary32/binary64 C++ types.
template <typename Value>
    requires(std::same_as<Value, float> || std::same_as<Value, double>)
struct NativeCodec<Value> {
    // Require a finite float term; reject overflow, underflow and precision-losing float narrowing.
    static CallResult<Value> decode(ProcessContext &context, const Term &term, ConversionBudget &budget);
    // Widen float exactly or retain double; reject non-finite native results.
    static CallResult<Term> encode(ProcessContext &context, Value value, ConversionBudget &budget);
};

// Map booleans to true/false atoms, without integer or truthiness coercion.
template <> struct NativeCodec<bool> {
    // Accept only the two Erlang boolean atoms.
    static CallResult<bool> decode(ProcessContext &context, const Term &term, ConversionBudget &budget);
    // Create the matching atom in the caller's context.
    static CallResult<Term> encode(ProcessContext &context, bool value, ConversionBudget &budget);
};

// Pass through a checked caller-owned root; no cross-process ownership transfer is implicit.
template <> struct NativeCodec<Term> {
    // Validate owner/liveness and retain the same immutable argument value.
    static CallResult<Term> decode(ProcessContext &context, const Term &term, ConversionBudget &budget);
    // Reject foreign/expired roots and return a rooted value in the current context.
    static CallResult<Term> encode(ProcessContext &context, const Term &value, ConversionBudget &budget);
};

// Map an owning UTF-8 string to an Erlang binary; embedded NUL is preserved by explicit length.
template <> struct NativeCodec<std::string> {
    // Copy a whole-byte binary and validate UTF-8 within the conversion budget.
    static CallResult<std::string> decode(ProcessContext &context, const Term &term, ConversionBudget &budget);
    // Validate UTF-8 and copy string bytes into a caller-owned binary.
    static CallResult<Term> encode(ProcessContext &context, const std::string &value, ConversionBudget &budget);
};

// Map Unicode scalar strings to Erlang character lists, distinct from UTF-8 binaries.
template <> struct NativeCodec<std::u32string> {
    // Decode a proper list of Unicode scalar integers, rejecting surrogates and invalid values.
    static CallResult<std::u32string> decode(ProcessContext &context, const Term &term, ConversionBudget &budget);
    // Encode Unicode scalars as a proper list, preserving order and embedded zero.
    static CallResult<Term> encode(ProcessContext &context, const std::u32string &value, ConversionBudget &budget);
};

namespace detail {
// Exclude non-owning initializer_list results even though they are not ranges::view types.
template <typename Value> inline constexpr bool is_initializer_list = false;
template <typename Value> inline constexpr bool is_initializer_list<std::initializer_list<Value>> = true;
} // namespace detail

// Admit iterable values with owned lifetime; custom ranges must uphold the ownership contract.
template <typename Container>
concept NativeSequence =
    std::ranges::input_range<const Container> && std::movable<Container> && !std::ranges::view<Container> &&
    !std::ranges::borrowed_range<Container> && !detail::is_initializer_list<Container> &&
    !std::same_as<Container, std::string> && !std::same_as<Container, std::u32string>;

// Decode only containers whose insertion preserves input order and multiplicity.
template <typename Container>
concept NativeAppendableSequence = NativeSequence<Container> && std::default_initializable<Container> &&
                                   requires(Container &container, std::ranges::range_value_t<Container> value) {
                                       container.push_back(std::move(value));
                                   };

// Encode any admitted iterable of supported values; decode when ordered append is available.
template <NativeSequence Container>
    requires NativeReturn<std::ranges::range_value_t<Container>>
struct NativeCodec<Container> {
    // Construct an owning container from a proper list; never silently truncate or reorder elements.
    static CallResult<Container> decode(ProcessContext &context, const Term &term, ConversionBudget &budget)
        requires NativeAppendableSequence<Container> && NativeArgument<std::ranges::range_value_t<Container>>;
    // Copy iteration order into a proper list, recursively encoding every element.
    static CallResult<Term> encode(ProcessContext &context, const Container &value, ConversionBudget &budget);
};

// Give fixed-size arrays an exact-length decoder instead of pretending they support append.
template <typename Value, std::size_t Size>
    requires NativeReturn<Value>
struct NativeCodec<std::array<Value, Size>> {
    // Require exactly Size elements; construct each decoded value without needing a default Term.
    static CallResult<std::array<Value, Size>> decode(ProcessContext &context, const Term &term,
                                                      ConversionBudget &budget)
        requires NativeArgument<Value>;
    // Produce one proper list in array order, including the empty-array case.
    static CallResult<Term> encode(ProcessContext &context, const std::array<Value, Size> &value,
                                   ConversionBudget &budget);
};
} // namespace erlang_aot::runtime
