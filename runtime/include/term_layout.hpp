#pragma once

// Private target-runtime layout reservations, not an allocator, public ABI or wire format.
// Trailing storage starts after each fixed prefix and needs checked allocation and object lifetimes.
#include "base_types.hpp"
#include "binary_heap_object.hpp"
#include "callable.hpp"
#include "terms.hpp"
#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <cstddef>
#include <memory>
#include <type_traits>

namespace erlang_aot::runtime::detail::layout {
using Bignum = boost::multiprecision::cpp_int;

struct alignas(Word) BoxHeader final {
    // Reserve low primary tag 00, then five kind bits, then the count of words AFTER this header.
    static constexpr unsigned BOXED_KIND_BITS = 5;
    static constexpr unsigned CONTENT_SHIFT = 2 + BOXED_KIND_BITS;
    // Future checked heap constructors encode this word; bitfields and union type-punning are forbidden.
    Word value_;
};

// Future runtime-owned C++ bignum storage requires explicit construction/destruction, not raw copying.
struct BignumCell final {
    // Identify an untraced multiprecision object; its limbs are owned by the C++ value.
    BoxHeader header_;
    // Respect the multiprecision backend's stronger native alignment; padding is not a traced slot.
    Bignum value_;
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
    Term remote_host_;
};

// Cons cells have no header: the list primary tag points to exactly two traceable terms.
struct alignas(Word) ConsCell final {
    // Trace the element and the arbitrary (possibly improper) tail independently.
    Term head_;
    Term tail_;
};

struct alignas(Word) TupleCell final {
    // Trailing storage contains arity Term slots; an empty tuple is reserved as an immediate.
    BoxHeader header_;
};

struct KeyValuePair final {
    // Trace both map-key and mapped-value slots.
    Term key;
    Term value;
};

struct alignas(Word) MapCell final {
    // Trailing storage contains KeyValuePair entries; the word count includes both slots per entry.
    BoxHeader header_;
};

struct alignas(Word) HeapBinaryCell final {
    // Identify untraced trailing Word data; the extent includes trailing_word_bits_ plus that data.
    BoxHeader header_;
    // Zero means a full final word; otherwise count the valid high bits in the final word.
    Word trailing_word_bits_;
};

struct alignas(Word) RefcBinaryCell final {
    // Shared ownership is runtime-private C++ state, never part of generated-code access.
    BoxHeader header_;
    // Future factories must use BinaryHeapObject::create; destruction releases the final owner.
    std::shared_ptr<BinaryHeapObject> binary_;
};

struct alignas(Word) ExternalFunctionCell final {
    // Identify module/name atom slots and an untraced arity word.
    BoxHeader header_;
    Term module_;
    Term function_;
    Word arity_;
};

struct alignas(Word) ClosureCell final {
    // Identify the private prefix followed by capture_count_ Term slots.
    BoxHeader header_;
    // Callable ownership/pinning remains a later module-service decision.
    std::weak_ptr<Callable> function_;
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
static_assert(sizeof(BoxHeader) == sizeof(Word));
static_assert(alignof(BoxHeader) == alignof(Word));
static_assert(std::is_standard_layout_v<Term>);
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
static_assert(sizeof(RefcBinaryCell) % sizeof(Word) == 0);
static_assert(sizeof(ClosureCell) % sizeof(Word) == 0);
} // namespace erlang_aot::runtime::detail::layout
