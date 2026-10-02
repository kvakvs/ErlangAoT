#pragma once
#include "v1.hpp"

namespace erlang_aot::abi::v1 {
// Success and inequality are selection outcomes; failure requires the checked context channel.
enum class Equality : std::uint8_t { unequal, equal, failure };
// Canonical empty containers have no payload and are shared by both target widths.
inline constexpr unsigned empty_tuple = 0x2b;
inline constexpr unsigned empty_list = 0x3b;
} // namespace erlang_aot::abi::v1

// Validate ownership/representation before comparing; later boxed equality extends this service.
std::uint8_t erlang_aot_exact_v1(void *context, erlang_aot::abi::v1::TermWord left,
                                 erlang_aot::abi::v1::TermWord right) noexcept;
