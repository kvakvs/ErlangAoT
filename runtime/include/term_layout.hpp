#pragma once

// REVIEW SKETCH ONLY: private target-runtime layouts, not a public API or wire format.
// No allocator, accessor, tag encoder or collector is implemented here. See terms.md.
#include "../include/base_types.hpp"
#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <cstddef>
#include <cstdint>
#include <type_traits>

using cpp_int = boost::multiprecision::cpp_int;

namespace erlang_aot::runtime::detail::layout {
// One traceable term reference; initially a boxed address, later a private tagged word.
// With tagged term implementation: Non-boxed terms can coexist in memory with
// headers (header terms are marking a start of a boxed term in memory)
struct alignas(Word) TermSlot final {
    // Zero is an invalid/uninitialized slot, never nil or another Erlang term.
    Word encoded;
};

// Private object kinds distinguish layouts; these numeric IDs are provisional.
// This is currently represented by 4 bits in the HeaderTag, raise alarm if more than 16
// enum elements are added.
enum class ObjectKind : std::uint8_t {
    tuple = 0, // corresponds to BEAM VM constant ARITYVAL=0
    native_record = 1,
    bignum_positive = 2,
    bignum_negative = 3,
    reference = 4,
    fun_closure = 5, // function or a closure with attached frozen values
    floating = 6,
    external_function = 7,
    refc_binary = 8, // a reference-counted pointer to a global binary heap object
    heap_binary = 9, // a locally heap-contained data blob
    sub_binary = 10,
    match_context = 11, // something created by binary matching?
    ext_pid = 12,
    ext_port = 13,
    ext_ref = 14,
    map = 15,
};

// Defines the type of contents of a boxed value
// This tag is appended via union to the arity value in higher bits.
using HeaderTag = struct header_tag_t {
    // TODO: Logic extracting the boxed object kind and arity should go here?
    ObjectKind kind_ : 4;
    TermTagPrimary tag_primary_header_ : 2; // this is always 'header', otherwise not used

    ObjectKind kind() const { return kind_; }
};

// Every allocation on heap is prefixed with a Header or is a Word-sized Term;
// GC state belongs in side metadata in this proposal.
// Tagged term implementation keeps this in 1 word: Fits kind in the HeaderTag field
struct alignas(Word) Header final {
    // Total allocated words, including this header, trailing data and end padding.
    // How many words the content spans AFTER the header word
    Word size_words_ : (ERL_WORD_BITS - 6);
    // The type of content
    HeaderTag tag_;

    static constexpr Header new_header(const ObjectKind kind, const std::size_t arity) {
        return {.size_words_ = arity, .tag_ = {.kind_ = kind, .tag_primary_header_ = TermTagPrimary::header}};
    }
};

// Nil is a distinct Word-sized Term value, it is not stored as a Boxed with a Header

// While small integers fit into a Word with tag bits, big integers are boxed with IntegerCell
// Big integers use a trailing array of Word magnitude limbs, least-significant first.
struct alignas(Word) IntegerCell final {
    // Header also contains the limb count and the sign
    Header header;

    // Unknown amount of following bignum limbs accessible via a const pointer
    const Word *limb_ptr(const std::size_t i) const { return reinterpret_cast<const Word *>(&header + i + 1); }

    // Unknown amount of following bignum limbs accessible via a pointer
    Word *limb_ptr(const std::size_t i) { return reinterpret_cast<Word *>(&header + i + 1); }

    static constexpr Header new_bignum(const cpp_int &input, Word *placement) {
        return Header::new_header((input >= 0) ? ObjectKind::bignum_positive : ObjectKind::bignum_negative, input);
    }
};

// Raw float bytes avoid platform-specific double field alignment in the heap layout.
// Erlang float corresponds to a 64-bit C/C++ double.
struct alignas(Word) FloatCell final {
    // Identify a float allocation with no traced fields.
    // Assert header always equals FloatHeader
    Header header;
    // IEEE 754 binary64 bytes in target-native order; access through copying/bit conversion.
    double value;
};

// Atom spelling and interning are runtime-wide; heap values contain only an opaque table key.
//
// In compact tagged term implementation atoms would fit into a single word together
// with kind bits and value bits.
struct alignas(Word) AtomCell final {
    // Identify an atom allocation with no process-heap references.
    Header header;
    // Immutable AtomStorage ID; initially dense, independent of future table compaction.
    Word atom_id;
};

// Pid, port and reference use separate kinds with the same private registry-key layout.
struct alignas(Word) IdentityCell final {
    // Distinguish identity semantics; registry metadata is not scanned as heap pointers.
    Header header;
    // Runtime-owned immutable identity record, independent of resource liveness.
    Word identity_id;
};

// A cons preserves an arbitrary tail, including an improper-list tail.
struct alignas(Word) ConsCell final {
    // Identify exactly two traced slots.
    Header header;
    // Trace the first element independently of its semantic category.
    TermSlot head;
    // Trace the remaining list or arbitrary terminal value.
    TermSlot tail;
};

// Tuple elements follow this prefix as arity consecutive TermSlots.
struct alignas(Word) TuplePrefix final {
    // Identify a variable-length tuple and bound all traced elements.
    Header header;
    // Element count, including zero for the empty tuple.
    Word arity;
};

// One flat-map entry; both key and value are ordinary traced terms.
struct alignas(Word) MapEntry final {
    // Exact Erlang equality determines key identity, not slot bit equality.
    TermSlot key;
    // Trace the associated immutable value.
    TermSlot value;
};

// Initially maps use count trailing MapEntry records; a tree layout can replace this privately.
struct alignas(Word) MapPrefix final {
    // Identify a flat map and bound its trailing storage.
    Header header;
    // Unique key count after exact-equality duplicate resolution.
    Word count;
};

// Packed bytes follow this prefix, with word padding after the last meaningful byte.
struct alignas(Word) BitstringPrefix final {
    // Identify untraced bit storage, including byte-sized binaries.
    Header header;
    // Logical length in bits; unused low bits in the final byte and padding are zero.
    Word bit_count;
};

// External functions retain names for later module resolution, not executable pointers.
struct alignas(Word) ExternalFunctionCell final {
    // Select scanning of the module and name slots only.
    Header header;
    // Trace atom terms naming the module and function.
    TermSlot module;
    TermSlot name;
    // Argument count, validated against the supported Erlang arity limit.
    Word arity;
};

// Closures have capture_count trailing TermSlots; descriptors live outside process heaps.
struct alignas(Word) ClosurePrefix final {
    // Identify the capture array and complete allocation extent.
    Header header;
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
    Header header;
    // Immutable registered module/record/schema identity, distinct from a tuple tag.
    Word descriptor_id;
    // Number of trailing traced fields, checked against the registered descriptor.
    Word field_count;
};

// Reject padding or layout drift at build time on every actual runtime target.
static_assert(std::is_standard_layout_v<TermSlot> && std::is_trivially_copyable_v<TermSlot>);
static_assert(sizeof(TermSlot) == sizeof(Word) && alignof(TermSlot) == alignof(Word));
static_assert(offsetof(TermSlot, encoded) == 0);
static_assert(sizeof(Header) == 2 * sizeof(Word));
static_assert(offsetof(Header, kind) == 0 && offsetof(Header, size_words) == sizeof(Word));
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
