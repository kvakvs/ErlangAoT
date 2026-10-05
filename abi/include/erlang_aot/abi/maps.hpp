#pragma once
#include "v1.hpp"

namespace erlang_aot::abi::v1 {
// from_list takes a proper list of {Key, Value} tuples (later keys win); key_at, value_at and iterator take a map and
// a zero-based position in its canonical key order, iterator returning OTP's {Key, Value, Next} chain ending in none.
enum class MapOperation : std::uint8_t {
    make,
    update,
    get,
    contains,
    size,
    test,
    from_list,
    key_at,
    value_at,
    iterator
};
// Semantic failures publish an owned offending key/map; infrastructure failures leave output untouched.
enum class MapOutcome : std::uint8_t { success, bad_map, bad_key, failure };
} // namespace erlang_aot::abi::v1

// Borrow rooted source-ordered arguments and publish one complete map/result or semantic error payload.
std::uint8_t erlang_aot_map_v1(void *context, std::uint8_t operation, const erlang_aot::abi::v1::TermWord *values,
                               std::size_t count, erlang_aot::abi::v1::TermWord *output) noexcept;
