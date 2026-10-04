#include "heap_walk.hpp"

namespace erlang_aot::runtime::detail {
namespace {
using layout::BoxHeader;
using Shape = HeapCell::Shape;

// Turn a size check into a parse result that says whether the payload holds terms.
std::expected<bool, WalkError> sized(bool valid, bool traced) {
    return valid ? std::expected<bool, WalkError>(traced) : std::unexpected(WalkError::bad_size);
}

// A heap binary's count must match the bit length stored in its first payload word.
bool heap_binary_sized(std::span<const Word> payload) {
    return !payload.empty() && payload[0] <= 8 * layout::heap_binary_bytes &&
           payload.size() == layout::heap_binary_payload_words(payload[0], sizeof(Word));
}

// Check the payload size each admitted kind requires and report whether its words are terms.
std::expected<bool, WalkError> traced(BoxedKind kind, std::span<const Word> payload) {
    switch (kind) {
    case BoxedKind::tuple:
        return true;
    case BoxedKind::map:
        return sized(payload.size() % 2 == 0, true);
    case BoxedKind::filler:
        return false;
    case BoxedKind::bignum:
        return sized(payload.size() >= 2, false);
    case BoxedKind::floating:
        return sized(payload.size() == layout::float_payload_words(sizeof(Word)), false);
    case BoxedKind::heap_binary:
        return sized(heap_binary_sized(payload), false);
    case BoxedKind::refc_binary:
        return sized(payload.size() == layout::refc_payload_words(sizeof(Word)), false);
    default:
        return std::unexpected(WalkError::unknown_kind);
    }
}
} // namespace

std::expected<HeapCell, WalkError> parse_cell(std::span<const Word> rest) noexcept {
    const auto first = rest.front();
    if (!is_header(first)) {
        if (rest.size() < 2) {
            return std::unexpected(WalkError::overrun);
        }
        return HeapCell{rest.first(2), rest.first(2), Shape::cons};
    }
    if (first == 0) {
        return HeapCell{rest.first(1), {}, Shape::filler};
    }
    const auto count = BoxHeader::count(first);
    if (count >= rest.size()) {
        return std::unexpected(WalkError::overrun);
    }
    const auto words = rest.first(count + 1);
    const auto kind = BoxHeader::kind(first);
    return traced(kind, words.subspan(1)).transform([&](bool slots) {
        return HeapCell{words, slots ? words.subspan(1) : std::span<const Word>{},
                        kind == BoxedKind::filler ? Shape::filler : Shape::boxed};
    });
}
} // namespace erlang_aot::runtime::detail
