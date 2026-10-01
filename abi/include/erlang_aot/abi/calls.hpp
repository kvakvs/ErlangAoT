#pragma once
#include "v1.hpp"

namespace erlang_aot::abi::v1 {
// Selection failures stay local control flow; only exhausted dispatch/body mismatch raises an error.
enum class ErrorReason : std::uint8_t { function_clause = 1, badmatch = 2 };
} // namespace erlang_aot::abi::v1

// Native C++ services borrow a live context; generated code never inspects its layout.
std::uint8_t erlang_aot_call_failed_v2(void *context) noexcept;
// Preserve the first error; badmatch admits only checked immediate payloads until rooted heaps exist.
std::uint8_t erlang_aot_raise_v2(void *context, erlang_aot::abi::v1::ErrorReason reason,
                                 erlang_aot::abi::v1::TermWord value) noexcept;
