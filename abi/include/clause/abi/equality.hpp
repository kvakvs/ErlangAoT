#pragma once
#include "v1.hpp"

namespace clause::abi::v1 {
// Success and inequality are selection outcomes; failure requires the checked context channel.
enum class Equality : std::uint8_t { unequal, equal, failure };
// Canonical empty containers have no payload and are shared by both target widths.
inline constexpr unsigned empty_tuple = 0x2b;
inline constexpr unsigned empty_list = 0x3b;
} // namespace clause::abi::v1

// Validate ownership/representation before bounded structural comparison of every admitted category.
std::uint8_t CLAUSE_exact_v1(void *context, clause::abi::v1::TermWord left, clause::abi::v1::TermWord right) noexcept;
