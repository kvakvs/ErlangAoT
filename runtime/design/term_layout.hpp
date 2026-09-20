#pragma once

// REVIEW SKETCH ONLY: private target-runtime layouts, not a public API or wire format.
// No allocator, accessor, tag encoder or collector is implemented here. See terms.md.
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace erlang_aot::runtime::detail::layout {
// Use the runtime target's pointer width, never the compiler host's width for cross emission.
using Word = std::uintptr_t;
static_assert(sizeof(Word) == 4 || sizeof(Word) == 8);
static_assert(alignof(Word) == sizeof(Word));

// One traceable term reference; initially a boxed address, later a private tagged word.
//
// With tagged term implementation: Non-boxed terms can coexist in memory with
// headers (header terms are marking a start of a boxed term in memory)
struct alignas(Word) TermSlot final {
    // Zero is an invalid/uninitialized slot, never nil or another Erlang term.
    Word encoded;
};

// Private object kinds distinguish layouts; these numeric IDs are provisional.
enum class ObjectKind : std::uint8_t {
    integer,
    floating,
    atom,
    reference,
    external_function,
    closure,
    port,
    pid,
    tuple,
    map,
    nil,
    cons,
    bitstring,
    native_record
};

// Every allocation begins here; GC state belongs in side metadata in this proposal.
// The header is a starting word of every memory structure or term, so size of this
// struct is critical for memory consumption.
//
// Tagged term implementation keeps this in 1 word: Fits kind in 2 4 or 6 bits
// (cascading based on previous 2 bits value) and word size in the remaining bits.
struct alignas(Word) Header final {
    // Word-encoded ObjectKind avoids implicit padding, C++ bitfields and hidden tag packing.
    Word kind;
    // Total allocated words, including this header, trailing data and end padding.
    Word size_words;
};

// Nil is a distinct value, not a null reference; it has no traced fields.
struct alignas(Word) NilCell final {
    // Identify this empty-list allocation and its complete extent.
    Header header;
};

// Arbitrary integers use a trailing array of Word magnitude limbs, least-significant first.
struct alignas(Word) IntegerPrefix final {
    // Select integer scanning (no term references) and bound the allocation.
    Header header;
    // Zero means nonnegative, one means negative; zero itself is always nonnegative.
    Word negative;
    // Number of trailing base-2^word_bits limbs; zero has no limbs.
    Word limb_count;
};

// Raw float bytes avoid platform-specific double field alignment in the heap layout.
// Erlang float corresponds to a 64-bit C/C++ double.
struct alignas(Word) FloatCell final {
    // Identify a float allocation with no traced fields.
    Header header;
    // IEEE 754 binary64 bytes in target-native order; access through copying/bit conversion.
    std::array<std::byte, 8> ieee754;
};

// Atom spelling and interning are runtime-wide; heap values contain only an opaque table key.
//
// In compact tagged term implementation atoms would fit into a single word together
// with kind bits and value bits.
struct alignas(Word) AtomCell final {
    // Identify an atom allocation with no process-heap references.
    Header header;
    // Stable entry in this runtime's atom table; not a public Erlang atom encoding.
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
static_assert(sizeof(IntegerPrefix) == 4 * sizeof(Word));
static_assert(offsetof(IntegerPrefix, negative) == 2 * sizeof(Word));
static_assert(offsetof(IntegerPrefix, limb_count) == 3 * sizeof(Word));
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
