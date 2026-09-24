#pragma once
#include "base_types.hpp"
#include <array>
#include <cstdint>
#include <expected>

namespace erlang_aot::runtime {
// Host API failures; these are not Erlang exception terms or generated-code ABI values.
enum class TermError : std::uint8_t {
    wrong_type,
    out_of_range,
    invalid_encoding,
    invalid_argument,
    improper_list,
    missing_key,
    unknown_field,
    wrong_owner,
    expired_context,
    resource_limit,
    not_implemented
};

struct TermTag {
    // Keep the encoded word intact; C++ bitfield order never defines the term ABI.
    Word value_;

    // Resolve the first non-delegating tag, including immediate empty tuples and lists.
    [[nodiscard]] constexpr TermKind get_kind() const noexcept {
        static constexpr std::array kinds{
            TermKind::header,    TermKind::list,         TermKind::boxed,       TermKind::invalid,
            TermKind::local_pid, TermKind::local_port,   TermKind::invalid,     TermKind::smallint,
            TermKind::atom,      TermKind::catch_object, TermKind::empty_tuple, TermKind::empty_list,
        };
        const auto primary = static_cast<unsigned>(value_ & abi::v1::primary_mask);
        const auto secondary = static_cast<unsigned>((value_ >> 2) & 3U);
        const auto tertiary = static_cast<unsigned>((value_ >> 4) & 3U);
        const auto use_secondary = static_cast<unsigned>(primary == 3U);
        const auto use_tertiary = use_secondary & static_cast<unsigned>(secondary == 2U);
        // Delegation advances index 3 to row 4, then index 6 to row 8; ignored fields contribute zero.
        const auto index = primary + use_secondary * (1U + secondary) + use_tertiary * (4U + tertiary - secondary);
        return kinds[index];
    }
};

static_assert(sizeof(TermTag) == sizeof(Word));
static_assert(alignof(TermTag) == alignof(Word));
static_assert((static_cast<unsigned>(TermKind2::smallint) << 2 |
               static_cast<unsigned>(TermKindPrimary::see_termkind2)) == abi::v1::small_integer_tag);

// Carry a checked value or failure without fabricating an Erlang result.
template <typename Value> using TermResult = std::expected<Value, TermError>;

// Inspect immediate tag structure only; atom/pid/port payloads do not establish runtime identity.
// Headers, catches and noncanonical empty values are invalid; list/boxed words return wrong_type.
[[nodiscard]] TermResult<TermKind> classify_immediate(Word value) noexcept;

// Encode a native-width small integer; values requiring bignums fail with out_of_range.
[[nodiscard]] TermResult<Word> encode_integer(std::int64_t value) noexcept;

// Decode only small integers; malformed immediates are invalid_encoding, other categories wrong_type.
[[nodiscard]] TermResult<std::int64_t> decode_integer(Word value) noexcept;
} // namespace erlang_aot::runtime
