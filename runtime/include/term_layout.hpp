#pragma once

// REVIEW SKETCH ONLY: private target-runtime layouts, not a public API or wire format.
// No allocator, accessor, tag encoder or collector is implemented here. See terms.md.
#include "../include/base_types.hpp"
#include "../include/binary_heap_object.hpp"
#include "../include/terms.hpp"
#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <cstddef>
#include <cstdint>
#include <type_traits>

using Bignum = boost::multiprecision::cpp_int;

namespace erlang_aot::runtime::detail::layout {

// Every allocation on heap is prefixed with a BoxHeader or is a Word-sized Term;
// GC state belongs in side metadata in this proposal.
// Tagged term implementation keeps this in 1 word: Fits kind in the BoxTag field
struct alignas(Word) BoxHeader final {
    // Defines the type of contents of a boxed value
    // This tag is appended via union to the arity value in higher bits.
    struct BoxTag {
        // TODO: Logic extracting the boxed object kind and arity should go here?
        BoxedKind boxed_kind_ : 5;
        TermKindPrimary tag_primary_header_ : 2; // this is always 'header', otherwise not used

        explicit constexpr BoxTag(const BoxedKind kind)
            : boxed_kind_(kind), tag_primary_header_(TermKindPrimary::header) {}

        BoxedKind boxed_kind() const { return boxed_kind_; }
    };

    // Total allocated words, after the header word, this should be consistent for different
    // cell types, to assist garbage collector.
    // How many words the content spans AFTER the header word
    Word arity_ : (ERL_WORD_BITS - 6);
    // The type of content is determined from this
    BoxTag tag_;

    constexpr BoxHeader(const BoxedKind kind, const std::size_t arity) : arity_(arity), tag_(kind) {}
};

// Nil is a distinct Word-sized Term value, it is not stored as a Boxed with a BoxHeader

// While small integers fit into a Word with tag bits, big integers are boxed with IntegerCell
// Big integers use a trailing array of Word magnitude limbs, least-significant first.
struct BignumCell final {
    // BoxHeader also contains the limb count and the sign
    BoxHeader header_;
    Bignum value_;

    explicit constexpr BignumCell(const Bignum &input) : header_(BoxedKind::bignum, 0), value_(input) {}
};

// Raw float bytes avoid platform-specific double field alignment in the heap layout.
// Erlang float corresponds to a 64-bit C/C++ double.
struct alignas(Word) FloatCell final {
    // Identify a float allocation with no traced fields.
    // Assert header always equals FloatHeader
    BoxHeader header_;
    // IEEE 754 binary64 bytes in target-native order; access through copying/bit conversion.
    double value_;

    explicit constexpr FloatCell(const double value) : header_(BoxedKind::floating, 0), value_(value) {}
};

// Pid, port and reference use separate kinds with the same private registry-key layout.
struct alignas(Word) RemoteIdentityCell final {
    // Distinguish identity semantics; registry metadata is not scanned as heap pointers.
    BoxHeader header_;
    // Runtime-owned immutable identity record, independent of resource liveness.
    Word identity_id_;
    // Atom name of the remote host
    Term remote_host_;

    explicit constexpr RemoteIdentityCell(const Word remote_id, const Term remote_host)
        : header_(BoxedKind::ext_pid, 0), identity_id_(remote_id), remote_host_(remote_host) {}
};

// A cons preserves a list head and an arbitrary tail, including an improper-list tail.
// A cons cell does not have a header word, each component of the cell is an independent Term.
struct alignas(Word) ConsCell final {
    // Trace the first element independently of its semantic category.
    Term head_;
    // Trace the remaining list or arbitrary terminal value.
    Term tail_;

    explicit constexpr ConsCell(const Term head, const Term tail) : head_(head), tail_(tail) {}
};

// Tuple elements follow this prefix as arity consecutive TermSlots.
struct alignas(Word) TupleCell final {
    // Identify a variable-length tuple and bound all traced elements.
    BoxHeader header_;
    // Unsized array of tuple elements, Erlang index starting at 1
    Term elements_[];

    explicit constexpr TupleCell(const std::size_t arity) : header_(BoxedKind::tuple, arity) {}

    explicit constexpr TupleCell(const std::span<Term> elements) : header_(BoxedKind::tuple, elements.size()) {
        std::copy(elements.begin(), elements.end(), elements_);
    }
};

// Initially maps use count trailing MapEntry records; a tree layout can replace this privately.
struct alignas(Word) MapCell final {
    // Identify a flat map and bound its trailing storage.
    BoxHeader header_;
    // Each entry is two Terms key and value, so step size is 2 Words. BoxHeader's `arity`
    // counts each array element of entries_ including keys and values.
    Term entries_[];
};

// Heap binary stores data right on heap in the cell.
// A newly made binary smaller or equal in size to HEAP_BINARY_THRESHOLD_WORDS will be onheap.
// Packed bytes follow this prefix, with word padding after the last meaningful byte.
struct alignas(Word) HeapBinaryCell final {
    // Identify untraced bit storage, including byte-sized binaries.
    // For heap binary the reasonable limit is 64 bytes, before the binary is converted to refc.
    BoxHeader header_;
    // Logical length of the last Word in bits.
    Word trailing_word_bits_;
    // Followed by 1 or more content Words.
    Word values_[];
};

// Refc binary holds a shared object that owns its data in a vector of Words.
// A newly made binary bigger than HEAP_BINARY_THRESHOLD_WORDS will become this.
struct alignas(Word) RefcBinaryCell final {
    // Identify untraced bit storage, including byte-sized binaries.
    BoxHeader header_;
    std::shared_ptr<BinaryHeapObject> binary_;
};

// External functions retain names for later module resolution, not executable pointers.
struct alignas(Word) ExternalFunctionCell final {
    // Select scanning of the module and name slots only.
    BoxHeader header;
    // Trace atom terms naming the module and function.
    TermSlot module;
    TermSlot name;
    // Argument count, validated against the supported Erlang arity limit.
    Word arity;
};

// Closures have capture_count trailing TermSlots; descriptors live outside process heaps.
struct alignas(Word) ClosurePrefix final {
    // Identify the capture array and complete allocation extent.
    BoxHeader header;
    // Immutable registered code/environment-schema identity, not a raw code pointer.
    Word descriptor_id;
    // Runtime-issued fun identity preserves equality independently of heap location.
    Word identity_id;
    // Number of traced captured values following this prefix.
    Word capture_count;
};

// Native-record fields follow this prefix; descriptor identity is part of the value.
struct alignas(Word) NativeRecordPrefix final {
    // Identify a record allocation and bound all field slots.
    BoxHeader header;
    // Immutable registered module/record/schema identity, distinct from a tuple tag.
    Word descriptor_id;
    // Number of trailing traced fields, checked against the registered descriptor.
    Word field_count;
};

// Reject padding or layout drift at build time on every actual runtime target.
static_assert(std::is_standard_layout_v<Term> && std::is_trivially_copyable_v<Term>);
static_assert(sizeof(TermSlot) == sizeof(Word) && alignof(TermSlot) == alignof(Word));
static_assert(offsetof(TermSlot, encoded) == 0);
static_assert(sizeof(BoxHeader) == 2 * sizeof(Word));
static_assert(offsetof(BoxHeader, kind) == 0 && offsetof(BoxHeader, size_words) == sizeof(Word));
static_assert(sizeof(NilCell) == 2 * sizeof(Word));
static_assert(sizeof(IntegerHeader) == 4 * sizeof(Word));
static_assert(offsetof(IntegerHeader, negative) == 2 * sizeof(Word));
static_assert(offsetof(IntegerHeader, limb_count) == 3 * sizeof(Word));
static_assert(sizeof(FloatCell) == 2 * sizeof(Word) + 8);
static_assert(offsetof(FloatCell, ieee754) == 2 * sizeof(Word));
static_assert(sizeof(AtomCell) == 3 * sizeof(Word));
static_assert(offsetof(AtomCell, atom_id) == 2 * sizeof(Word));
static_assert(sizeof(IdentityCell) == 3 * sizeof(Word));
static_assert(offsetof(IdentityCell, identity_id) == 2 * sizeof(Word));
static_assert(sizeof(ConsCell) == 4 * sizeof(Word));
static_assert(offsetof(ConsCell, head) == 2 * sizeof(Word));
static_assert(offsetof(ConsCell, tail) == 3 * sizeof(Word));
static_assert(sizeof(TuplePrefix) == 3 * sizeof(Word));
static_assert(offsetof(TuplePrefix, arity) == 2 * sizeof(Word));
static_assert(sizeof(MapEntry) == 2 * sizeof(Word));
static_assert(offsetof(MapEntry, key) == 0 && offsetof(MapEntry, value) == sizeof(Word));
static_assert(sizeof(MapPrefix) == 3 * sizeof(Word));
static_assert(offsetof(MapPrefix, count) == 2 * sizeof(Word));
static_assert(sizeof(BitstringPrefix) == 3 * sizeof(Word));
static_assert(offsetof(BitstringPrefix, bit_count) == 2 * sizeof(Word));
static_assert(sizeof(ExternalFunctionCell) == 5 * sizeof(Word));
static_assert(offsetof(ExternalFunctionCell, module) == 2 * sizeof(Word));
static_assert(offsetof(ExternalFunctionCell, name) == 3 * sizeof(Word));
static_assert(offsetof(ExternalFunctionCell, arity) == 4 * sizeof(Word));
static_assert(sizeof(ClosurePrefix) == 5 * sizeof(Word));
static_assert(offsetof(ClosurePrefix, descriptor_id) == 2 * sizeof(Word));
static_assert(offsetof(ClosurePrefix, identity_id) == 3 * sizeof(Word));
static_assert(offsetof(ClosurePrefix, capture_count) == 4 * sizeof(Word));
static_assert(sizeof(NativeRecordPrefix) == 4 * sizeof(Word));
static_assert(offsetof(NativeRecordPrefix, descriptor_id) == 2 * sizeof(Word));
static_assert(offsetof(NativeRecordPrefix, field_count) == 3 * sizeof(Word));
} // namespace erlang_aot::runtime::detail::layout
