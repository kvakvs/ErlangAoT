#pragma once
#include "base_types.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
class ProcessContext;
class ProcessHeap;
class AtomStorage;
class ProcessIdentity;
class PortIdentity;
class ReferenceIdentity;
class FunctionIdentity;
class ClosureDescriptor;
class NativeRecordDescriptor;
class TermFactory;

// Identify an atom within its runtime; word-sized IDs remain stable across future storage compaction.
using AtomId = std::uintptr_t;

// One-word value API; currently only small integers and empty containers are constructible.
// Heap/identity operations below remain reserved until roots and runtime ownership exist.
class Term final {
  public:
    // Reserve zero as an invalid slot until checked immediate construction supplies a value.
    Term() : value_(0) {}

    // Copy/move the immediate value; heap ownership is deferred to step 12.
    Term(const Term &other) = default;
    Term(Term &&other) noexcept = default;
    // Rebind only this host handle, leaving every other alias unchanged.
    Term &operator=(const Term &other) = default;
    Term &operator=(Term &&other) noexcept = default;
    // Immediate-only handles have no roots or process-owned storage.
    ~Term() = default;

    // Admit only small integers and canonical empty containers; identities/heap values remain unavailable.
    static TermResult<Term> from_word(Word value) noexcept;
    // Expose the immediate representation for the generated service bridge.
    Word word() const noexcept;

    // Remaining semantic/heap operations below are reserved unless documented as implemented.
    // Copy the reachable value graph into destination storage and return a destination-owned root.
    TermResult<Term> copy_to(ProcessHeap &destination) const;

    // Identify the semantic category; binaries are byte-sized bitstrings.
    TermKind kind() const;

    // Test numeric categories without exposing small-integer/bignum storage.
    bool is_integer() const;
    bool is_float() const;
    bool is_number() const;
    // Test atoms, including the true/false convenience subset.
    bool is_atom() const;
    bool is_boolean() const;
    // Test opaque identity and callable categories.
    bool is_reference() const;
    bool is_function() const;
    bool is_function(std::size_t arity) const;
    bool is_port() const;
    bool is_pid() const;
    // Test container categories without exposing their storage.
    bool is_tuple() const;
    bool is_map() const;
    bool is_nil() const;
    bool is_cons() const;
    // Match Erlang is_list/1 (nil or cons); properness is a separate traversal.
    bool is_list() const;
    TermResult<bool> is_proper_list() const;
    // Test bitstrings and their whole-byte subset.
    bool is_bitstring() const;
    bool is_binary() const;
    // Distinguish native records from traditional tuple-backed records.
    bool is_native_record() const;
    bool is_native_record(const NativeRecordDescriptor &descriptor) const;

    // Extract bounded or lossless decimal integer values; narrowing checks range.
    TermResult<std::int64_t> integer_value() const;
    TermResult<std::string> integer_decimal() const;
    // Extract a float without coercing an integer.
    TermResult<double> float_value() const;
    // Copy the atom's Unicode spelling as UTF-8, or extract true/false.
    TermResult<std::string> atom_utf8() const;
    // Extract the atom's stable runtime-local number, not an integer Term or a storage address.
    TermResult<AtomId> atom_id() const;
    TermResult<bool> boolean_value() const;

    // Extract opaque identities, never process pointers, numeric IDs or native callbacks.
    TermResult<ProcessIdentity> pid_value() const;
    TermResult<PortIdentity> port_value() const;
    TermResult<ReferenceIdentity> reference_value() const;
    TermResult<FunctionIdentity> function_value() const;
    // Inspect callable arity without invoking it or exposing its environment storage.
    TermResult<std::size_t> function_arity() const;

    // Extract a cons cell; tail may be any term, including an improper-list tail.
    TermResult<Term> head() const;
    TermResult<Term> tail() const;
    // Traverse a proper list; report improper_list for a non-nil terminal tail.
    TermResult<std::size_t> list_length() const;
    TermResult<std::vector<Term>> list_elements() const;
    // Grow a list by returning a new value in the same process context.
    TermResult<Term> prepend(const Term &element) const;
    TermResult<Term> append(const Term &element) const;
    // Replace a zero-based element in a proper list, preserving the original.
    TermResult<Term> with_list_element(std::size_t index, const Term &element) const;

    // Inspect tuple size and elements using zero-based C++ indices.
    TermResult<std::size_t> tuple_size() const;
    TermResult<Term> tuple_element(std::size_t index) const;
    TermResult<std::vector<Term>> tuple_elements() const;
    // Replace one tuple element without modifying existing aliases.
    TermResult<Term> with_tuple_element(std::size_t index, const Term &element) const;

    // Inspect maps with exact Erlang key equality; absence is an empty optional.
    TermResult<std::size_t> map_size() const;
    TermResult<bool> map_contains(const Term &key) const;
    TermResult<std::optional<Term>> map_find(const Term &key) const;
    TermResult<std::vector<std::pair<Term, Term>>> map_entries() const;
    // Insert/replace (=>), replace-existing-only (:=), or remove a key immutably.
    TermResult<Term> with_map_entry(const Term &key, const Term &value) const;
    TermResult<Term> with_existing_map_entry(const Term &key, const Term &value) const;
    TermResult<Term> without_map_entry(const Term &key) const;

    // Copy packed MSB-first bits; unused low bits in the final byte are zero.
    TermResult<std::size_t> bit_size() const;
    TermResult<std::vector<std::byte>> bitstring_bytes() const;
    // Extract bytes only if the bitstring is a binary.
    TermResult<std::vector<std::byte>> binary_bytes() const;
    // Return a checked bit slice or concatenation in the same process context.
    TermResult<Term> bit_slice(std::size_t offset, std::size_t count) const;
    TermResult<Term> concat_bits(const Term &suffix) const;

    // Inspect registered native-record identity and fields by atom name.
    TermResult<NativeRecordDescriptor> record_descriptor() const;
    TermResult<Term> record_field(const Term &name) const;
    TermResult<std::vector<std::pair<Term, Term>>> record_fields() const;
    // Replace an existing field, preserving the descriptor and other values.
    TermResult<Term> with_record_field(const Term &name, const Term &value) const;

    // Compare values using Erlang exact equality, not C++ handle identity.
    TermResult<bool> exactly_equal(const Term &other) const;

  private:
    friend class TermFactory;
    friend class AtomStorage;

    // Store immediates or future tagged heap pointers; roots/owners require external runtime metadata.
    Word value_;
};

static_assert(sizeof(Term) == sizeof(Word));
static_assert(alignof(Term) == alignof(Word));

} // namespace erlang_aot::runtime
