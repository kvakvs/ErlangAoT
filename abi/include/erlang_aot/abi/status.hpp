#pragma once
#include <cstdint>

namespace erlang_aot::abi::v1 {
// Keep project service failures typed and numerically stable; never substitute a term for an error.
enum class Status : std::uint8_t {
    ok = 0,
    not_implemented = 1,
    invalid_argument = 2,
    diagnostic_failure = 3,
    out_of_memory = 4,
    busy = 5,
    wrong_owner = 6,
    resource_limit = 7,
    stopped = 8,
    abi_mismatch = 9,
    internal_error = 10
};
} // namespace erlang_aot::abi::v1
