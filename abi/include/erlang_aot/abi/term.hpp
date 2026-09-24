#pragma once
#include "v1.hpp"
#include <cstdint>
#include <expected>
#include <type_traits>

namespace erlang_aot::abi::v1 {
enum class IntegerError : std::uint8_t { out_of_range, wrong_tag };

// Select the emitted target width explicitly, independently of this C++ compiler's pointers.
template <unsigned Bits>
    requires(Bits == 32 || Bits == 64)
struct IntegerEncoding {
    using Word = std::conditional_t<Bits == 32, std::uint32_t, std::uint64_t>;
    // Four low bits identify an immediate integer; the remaining payload is signed.
    static constexpr unsigned tag_bits = small_integer_bits;
    static constexpr Word tag = small_integer_tag;
    static constexpr Word payload_mask = static_cast<Word>(~Word{0}) >> tag_bits;
    // Keep checked bounds in int64_t so both target widths accept the same exact input type.
    static constexpr std::int64_t minimum = -(std::int64_t{1} << (Bits - tag_bits - 1));
    static constexpr std::int64_t maximum = -minimum - 1;

    // Reject overflow before shifting an unsigned value; no signed shift or wrapping source semantics.
    static constexpr std::expected<Word, IntegerError> encode(std::int64_t value) noexcept {
        if (value < minimum || value > maximum) {
            return std::unexpected(IntegerError::out_of_range);
        }
        return static_cast<Word>((static_cast<Word>(value) << tag_bits) | tag);
    }

    // Reject other term kinds and reconstruct negatives without out-of-range unsigned-to-signed casts.
    static constexpr std::expected<std::int64_t, IntegerError> decode(Word value) noexcept {
        if ((value & tag) != tag) {
            return std::unexpected(IntegerError::wrong_tag);
        }
        const Word payload = value >> tag_bits;
        if (payload <= static_cast<Word>(maximum)) {
            return static_cast<std::int64_t>(payload);
        }
        return -1 - static_cast<std::int64_t>(payload_mask - payload);
    }
};

// Native runtime consumers share the contract; cross compilers must select 32 or 64 explicitly.
using NativeIntegerEncoding = IntegerEncoding<sizeof(TermWord) * 8>;
} // namespace erlang_aot::abi::v1
