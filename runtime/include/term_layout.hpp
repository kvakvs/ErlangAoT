#pragma once

// REVIEW SKETCH ONLY: private target-runtime layouts, not a public API or wire format.
// No allocator, accessor, tag encoder or collector is implemented here. See terms.md.
#include "../include/base_types.hpp"
#include "../include/binary_heap_object.hpp"
#include "../include/callable.hpp"
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
    static constexpr std::size_t BOXED_KIND_BITS = 5;

    // Defines the type of contents of a boxed value
    // This tag is appended via union to the arity value in higher bits.
    struct BoxTag {
        // TODO: Logic extracting the boxed object kind and arity should go here?
        BoxedKind boxed_kind_ : BOXED_KIND_BITS;
        TermKindPrimary tag_primary_header_ : 2; // this is always 'header', otherwise not used

        explicit constexpr BoxTag(const BoxedKind kind)
            : boxed_kind_(kind), tag_primary_header_(TermKindPrimary::header) {}

        BoxedKind boxed_kind() const { return boxed_kind_; }
    };

    // Total allocated words, after the header word, this should be consistent for different
    // cell types, to assist garbage collector.
    // How many words the content spans AFTER the header word
    Word arity_ : (ERL_WORD_BITS - BOXED_KIND_BITS - 2);
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

    explicit constexpr BignumCell(const Bignum &input)
        : header_(BoxedKind::bignum, sizeof(BignumCell) / sizeof(Word)), value_(input) {}
};

// Raw float bytes avoid platform-specific double field alignment in the heap layout.
// Erlang float corresponds to a 64-bit C/C++ double.
struct alignas(Word) FloatCell final {
    // Identify a float allocation with no traced fields.
    // Assert header always equals FloatHeader
    BoxHeader header_;
    // IEEE 754 binary64 bytes in target-native order; access through copying/bit conversion.
    double value_;

    explicit constexpr FloatCell(const double value)
        : header_(BoxedKind::floating, sizeof(FloatCell) / sizeof(Word)), value_(value) {}
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
        : header_(BoxedKind::ext_pid, sizeof(RemoteIdentityCell) / sizeof(Word)), identity_id_(remote_id),
          remote_host_(remote_host) {}
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

    explicit constexpr TupleCell(const std::size_t arity)
        : header_(BoxedKind::tuple, sizeof(TupleCell) / sizeof(Word) + arity) {}

    explicit constexpr TupleCell(const std::span<Term> elements)
        : header_(BoxedKind::tuple, sizeof(TupleCell) / sizeof(Word) + elements.size()) {
        std::copy(elements.begin(), elements.end(), elements_);
    }
};

using KeyValuePair = struct {
    Term key;
    Term value;
};

// Initially maps use count trailing MapEntry records; a tree layout can replace this privately.
struct alignas(Word) MapCell final {
    // Identify a flat map and bound its trailing storage.
    BoxHeader header_;

    // Each entry is two Terms key and value, so step size is 2 Words. BoxHeader's `arity`
    // counts each array element of key_value_pairs_ including keys and values.
    KeyValuePair key_value_pairs_[];

    explicit constexpr MapCell(const std::span<KeyValuePair> key_value_pairs)
        : header_(BoxedKind::map, sizeof(MapCell) / sizeof(Word) + key_value_pairs.size() * 2) {}
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

    explicit constexpr HeapBinaryCell(const std::span<const Word> values, Word trailing_word_bits = 0)
        : header_(BoxedKind::heap_binary, values.size() + sizeof(HeapBinaryCell) / sizeof(Word)),
          trailing_word_bits_(trailing_word_bits) {
        std::copy(values.begin(), values.end(), values_);
    }
};

// Refc binary holds a shared object that owns its data in a vector of Words, the object
// is freed automatically when last user forgets about it.
// A newly made binary bigger than HEAP_BINARY_THRESHOLD_WORDS will become this.
struct alignas(Word) RefcBinaryCell final {
    // Identify untraced bit storage, including byte-sized binaries.
    BoxHeader header_;
    std::shared_ptr<BinaryHeapObject> binary_;

    explicit constexpr RefcBinaryCell(const std::span<const Word> values, Word trailing_word_bits = 0)
        : header_(BoxedKind::refc_binary, sizeof(RefcBinaryCell) / sizeof(Word)),
          binary_(std::make_shared<BinaryHeapObject>(values, trailing_word_bits)) {
        // TODO: Call BinaryHeapObject::create and unwrap the result to store shared ptr
    }
};

// External functions retain names for later module resolution, not executable pointers.
struct alignas(Word) ExternalFunctionCell final {
    // Select scanning of the module and name slots only.
    BoxHeader header_;
    // Trace atom terms naming the module and function.
    Term module_;
    Term function_;
    // Argument count, validated against the supported Erlang arity limit.
    Word arity_;

    explicit constexpr ExternalFunctionCell(const Term module, const Term function, const Word arity)
        : header_(BoxedKind::external_function, sizeof(ExternalFunctionCell) / sizeof(Word)), module_(module),
          function_(function), arity_(arity) {}
};

// Closures have capture_count trailing TermSlots; descriptors live outside process heaps.
struct alignas(Word) ClosureCell final {
    // Identify the capture array and complete allocation extent.
    BoxHeader header_;
    // Investigate whether this needs to be a weak_ptr or we can go with shared_ptr or raw pointer
    std::weak_ptr<Callable> function_;
    // Number of traced captured values following this prefix.
    Word capture_count_;
    Term captured_values_[];
};

// Native-record fields follow this prefix; descriptor identity is part of the value.
struct alignas(Word) NativeRecordPrefix final {
    // Identify a record allocation and bound all field slots.
    BoxHeader header_;
    // Immutable registered schema identity, distinct from a tuple tag.
    // TODO: Record registry to store schemas
    Word descriptor_;
    // Number of trailing traced fields, checked against the registered descriptor.
    Word field_count_;
    Term field_[];
};

} // namespace erlang_aot::runtime::detail::layout
