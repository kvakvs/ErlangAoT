#pragma once

// Private target-runtime cell layouts (docs/runtime-heap.md), not an allocator, public ABI or wire format.
// Trailing storage starts after each fixed prefix; every cell moves by copying words except RefcBinaryCell.
#include "terms.hpp"
#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <cstddef>
#include <erlang_aot/runtime/base_types.hpp>
#include <memory>
#include <type_traits>
#include <vector>

namespace erlang_aot::runtime::detail::layout {
using Bignum = boost::multiprecision::cpp_int;

struct alignas(Word) BoxHeader final {
    // Reserve low primary tag 00, then five kind bits, then the count of words AFTER this header.
    static constexpr unsigned BOXED_KIND_BITS = 5;
    static constexpr unsigned CONTENT_SHIFT = 2 + BOXED_KIND_BITS;
    // Future checked heap constructors encode this word; bitfields and union type-punning are forbidden.
    Word value_;

    // Encode a header for kind followed by count words.
    static constexpr Word make(BoxedKind kind, std::size_t count) noexcept {
        return (static_cast<Word>(count) << CONTENT_SHIFT) | (static_cast<Word>(kind) << 2);
    }

    // Decode the kind bits of a header word.
    static constexpr BoxedKind kind(Word header) noexcept {
        return static_cast<BoxedKind>((header >> 2) & ((1U << BOXED_KIND_BITS) - 1));
    }

    // Decode the number of words following a header word.
    static constexpr std::size_t count(Word header) noexcept {
        return static_cast<std::size_t>(header >> CONTENT_SHIFT);
    }
};

// Canonical magnitude limbs follow this private prefix in least-significant-word order.
struct alignas(Word) BignumCell final {
    // Identify the untraced sign word and following immutable native-word limbs.
    BoxHeader header_;
    // Zero denotes positive and one denotes negative; zero itself always uses a small integer.
    Word negative_;
};

struct alignas(Word) FloatCell final {
    // Identify untraced IEEE binary64 bytes without depending on the target's double alignment.
    BoxHeader header_;
    std::array<std::byte, 8> value_;
};

struct alignas(Word) RemoteIdentityCell final {
    // Distinguish identity kinds; the registry key is not a pointer for the future GC.
    BoxHeader header_;
    Word identity_id_;
    // Trace the remote host atom independently of the registry key.
    Word remote_host_;
};

// Cons cells have no header: the list primary tag points to exactly two traceable terms.
struct alignas(Word) ConsCell final {
    // Trace the element and the arbitrary (possibly improper) tail independently.
    Word head_;
    Word tail_;
};

struct alignas(Word) TupleCell final {
    // Trailing storage contains arity Term slots; an empty tuple is reserved as an immediate.
    BoxHeader header_;
};

struct KeyValuePair final {
    // Trace both map-key and mapped-value slots.
    Word key;
    Word value;
};

struct alignas(Word) MapCell final {
    // Trailing storage contains KeyValuePair entries; the word count includes both slots per entry.
    BoxHeader header_;
};

// Packed MSB-first binary data shared by off-heap cells; immutable once published.
using BinaryBuffer = std::vector<std::byte>;

// Binaries up to this many bytes live inline on the heap; larger ones use a shared off-heap buffer.
inline constexpr std::size_t heap_binary_bytes = HEAP_BINARY_THRESHOLD_WORDS * sizeof(Word);

struct alignas(Word) HeapBinaryCell final {
    // Identify untraced trailing data words; the header count is 1 plus the rounded-up data words.
    BoxHeader header_;
    // Exact logical length in bits; data bytes follow, zero padded after the last valid bit.
    Word bits_;
};

struct alignas(Word) RefcBinaryCell final {
    // Identify an off-heap binary view (BEAM ProcBin); the header count is 5 on every word width.
    BoxHeader header_;
    // Select this view's bits within the shared buffer.
    Word offset_;
    Word bits_;
    // Hold one reference to the buffer; only the off-heap list sweep, teardown or relocation touches it.
    std::shared_ptr<const BinaryBuffer> buffer_;
    // Link the owning process's off-heap list, newest cell first; null ends the list.
    RefcBinaryCell *next_;
};

struct alignas(Word) ExternalFunctionCell final {
    // Identify module/name atom slots and an untraced arity word.
    BoxHeader header_;
    Word module_;
    Word function_;
    Word arity_;
};

struct alignas(Word) ClosureCell final {
    // Identify the private prefix followed by capture_count_ Term slots.
    BoxHeader header_;
    // Untraced registry ID of the callable; pinning remains a later module-service decision.
    Word function_;
    Word capture_count_;
};

struct alignas(Word) NativeRecordPrefix final {
    // Identify the prefix followed by field_count_ Term slots.
    BoxHeader header_;
    // Registry identity is untraced; each trailing field is traced.
    Word descriptor_;
    Word field_count_;
};

static_assert(static_cast<unsigned>(BoxedKind::empty_list) < (1U << BoxHeader::BOXED_KIND_BITS));
static_assert(BoxHeader::kind(BoxHeader::make(BoxedKind::refc_binary, 5)) == BoxedKind::refc_binary);
static_assert(BoxHeader::count(BoxHeader::make(BoxedKind::map, 6)) == 6);
static_assert(sizeof(BoxHeader) == sizeof(Word));
static_assert(alignof(BoxHeader) == alignof(Word));
static_assert(sizeof(ConsCell) == 2 * sizeof(Word));
static_assert(offsetof(ConsCell, tail_) == sizeof(Word));
static_assert(sizeof(TupleCell) == sizeof(Word));
static_assert(sizeof(MapCell) == sizeof(Word));
static_assert(sizeof(KeyValuePair) == 2 * sizeof(Word));
static_assert(sizeof(FloatCell) == sizeof(Word) + 8);
static_assert(offsetof(FloatCell, value_) == sizeof(Word));
static_assert(sizeof(RemoteIdentityCell) == 3 * sizeof(Word));
static_assert(sizeof(HeapBinaryCell) == 2 * sizeof(Word));
static_assert(sizeof(ExternalFunctionCell) == 4 * sizeof(Word));
static_assert(sizeof(NativeRecordPrefix) == 3 * sizeof(Word));
static_assert(sizeof(BignumCell) % sizeof(Word) == 0);
static_assert(alignof(BignumCell) >= alignof(Word));
static_assert(sizeof(ClosureCell) == 3 * sizeof(Word));
// The shared_ptr is the only C++ member a cell may hold; it is two pointers on every supported STL.
static_assert(sizeof(std::shared_ptr<const BinaryBuffer>) == 2 * sizeof(Word));
static_assert(alignof(std::shared_ptr<const BinaryBuffer>) <= alignof(Word));
static_assert(sizeof(RefcBinaryCell) == 6 * sizeof(Word));
static_assert(!std::is_trivially_copyable_v<RefcBinaryCell>);
static_assert(std::is_nothrow_move_constructible_v<RefcBinaryCell>);
// Every other cell moves by copying its words.
static_assert(std::is_trivially_copyable_v<BoxHeader> && std::is_trivially_copyable_v<BignumCell> &&
              std::is_trivially_copyable_v<FloatCell> && std::is_trivially_copyable_v<RemoteIdentityCell> &&
              std::is_trivially_copyable_v<ConsCell> && std::is_trivially_copyable_v<TupleCell> &&
              std::is_trivially_copyable_v<KeyValuePair> && std::is_trivially_copyable_v<MapCell> &&
              std::is_trivially_copyable_v<HeapBinaryCell> && std::is_trivially_copyable_v<ExternalFunctionCell> &&
              std::is_trivially_copyable_v<ClosureCell> && std::is_trivially_copyable_v<NativeRecordPrefix>);
} // namespace erlang_aot::runtime::detail::layout
