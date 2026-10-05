#pragma once
#include "v1.hpp"
#include <cstddef>

namespace erlang_aot::abi::v1 {
// concat joins a proper list of bitstrings in order; any other element is a bad argument.
enum class BitOperation : std::uint8_t { make, extract, test, finish, part, concat };
enum class BitType : std::uint8_t { integer, floating, binary, utf8, utf16, utf32 };
// Pack segment metadata independently of term layout; native endian is resolved by the emitted target.
inline constexpr unsigned bit_little = 8;
inline constexpr unsigned bit_signed = 16;
inline constexpr unsigned bit_all = 32;
inline constexpr unsigned bit_empty = 64;
} // namespace erlang_aot::abi::v1

// Borrow rooted arguments; publish a term and encoded next cursor only on success.
std::uint8_t erlang_aot_bits_v1(void *context, std::uint8_t operation, const erlang_aot::abi::v1::TermWord *values,
                                std::size_t count, erlang_aot::abi::v1::TermWord *output) noexcept;
