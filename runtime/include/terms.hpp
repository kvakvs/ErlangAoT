#pragma once

// REVIEW SKETCH ONLY: API declarations and a tag decoder, excluded from compilation by the build.
// See terms.md for ownership, errors, immutable updates and the private ABI boundary.
#include "../include/base_types.hpp"
#include <array>
#include <cstddef>
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
    // Reserve the payload bits outside the three tag fields.
    Word _padding : (ERL_WORD_BITS - 6);

    // Third-level leaf, meaningful only when both preceding levels delegate.
    TermKind3 tag3_ : 2;
    // Second-level leaf or delegation to tag3_, selected by tag_primary_.
    TermKind2 tag2_ : 2;
    // First-level category or delegation to tag2_.
    TermKindPrimary tag_primary_ : 2;

    // Resolve the first non-delegating tag, including immediate empty tuples and lists.
    [[nodiscard]] constexpr TermKind get_kind() const noexcept {
        static constexpr std::array kinds{
            TermKind::header,    TermKind::list,         TermKind::boxed,       TermKind::invalid,
            TermKind::local_pid, TermKind::local_port,   TermKind::invalid,     TermKind::smallint,
            TermKind::atom,      TermKind::catch_object, TermKind::empty_tuple, TermKind::empty_list,
        };
        const auto primary = static_cast<unsigned>(tag_primary_);
        const auto secondary = static_cast<unsigned>(tag2_);
        const auto tertiary = static_cast<unsigned>(tag3_);
        const auto use_secondary = static_cast<unsigned>(tag_primary_ == TermKindPrimary::see_termkind2);
        const auto use_tertiary = use_secondary & static_cast<unsigned>(tag2_ == TermKind2::see_termkind3);
        // Delegation advances index 3 to row 4, then index 6 to row 8; ignored fields contribute zero.
        const auto index = primary + use_secondary * (1U + secondary) + use_tertiary * (4U + tertiary - secondary);
        return kinds[index];
    }
};

// Carry a checked value or failure without fabricating an Erlang result.
template <typename Value> using TermResult = std::expected<Value, TermError>;

// Common value class for every Erlang term; payload classes and representation stay private.
// Tagged Term Implementation: Stores term kind in the value_ word lowest 2, 4 or 6 bits.
// Important property: Term is a pointer-sized (Word-sized) object passable by value, but boxed
// terms contain bits of a memory pointer, which can be resolved into a boxed term of some kind.
class Term final {
  public:
    Term() : value_(0) {}

    // Copy retains the same immutable value; move transfers this host handle.
    Term(const Term &other);
    Term(Term &&other) noexcept;
    // Rebind only this host handle, leaving every other alias unchanged.
    Term &operator=(const Term &other);
    Term &operator=(Term &&other) noexcept;
    // Release this handle's ownership/root registration.
    ~Term();

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

    union {
        Word value_;
        TermTag tag_;
    };

    // // Hide process binding, resource budgets and allocation policy from consumers.
    // class Impl;
    // std::unique_ptr<Impl> impl_;
};

// Create terms owned by one live process; inputs are copied/retained before return.
class TermFactory final {
  public:
    // Bind allocation and root registration to an existing process context.
    explicit TermFactory(ProcessContext &context);
    // Release the factory's context binding; returned terms retain their own roots.
    ~TermFactory();
    // Keep one factory binding; moving transfers it without moving the process.
    TermFactory(TermFactory &&other) noexcept;
    TermFactory &operator=(TermFactory &&other) noexcept;
    // Forbid accidental copying of the process-bound factory.
    TermFactory(const TermFactory &) = delete;
    TermFactory &operator=(const TermFactory &) = delete;

    // Construct machine-sized or arbitrary-precision integers (optional sign, decimal digits).
    TermResult<Term> integer(std::int64_t value);
    TermResult<Term> integer_decimal(std::string_view value);
    // Construct a finite Erlang float; reject NaN and infinity.
    TermResult<Term> floating(double value);
    // Delegate interning to the context's runtime AtomStorage; boolean uses its true/false entries.
    TermResult<Term> atom(std::string_view utf8);
    TermResult<Term> boolean(bool value);

    // Construct an empty list, a cons with any tail, or a proper list in input order.
    TermResult<Term> nil();
    TermResult<Term> cons(const Term &head, const Term &tail);
    TermResult<Term> list(std::span<const Term> elements);
    // Construct a tuple, including the zero-element tuple.
    TermResult<Term> tuple(std::span<const Term> elements);
    // Construct a map; the last input entry wins for an exactly equal key.
    TermResult<Term> map(std::span<const std::pair<Term, Term>> entries);
    // Copy bytes or an explicitly sized MSB-first sequence of bits.
    TermResult<Term> binary(std::span<const std::byte> bytes);
    TermResult<Term> bitstring(std::span<const std::byte> bytes, std::size_t bit_count);

    // Wrap runtime-issued identities; constructing a term does not spawn/open a resource.
    TermResult<Term> pid(const ProcessIdentity &identity);
    TermResult<Term> port(const PortIdentity &identity);
    TermResult<Term> reference(const ReferenceIdentity &identity);
    // Obtain a fresh unique reference from the owning runtime.
    TermResult<Term> make_reference();
    // Construct an external fun from module/name atoms and a validated arity.
    TermResult<Term> external_function(const Term &module, const Term &name, std::size_t arity);
    // Bind captures to a runtime-registered closure descriptor, never a raw C++ callback.
    TermResult<Term> closure(const ClosureDescriptor &descriptor, std::span<const Term> captures);
    // Rewrap an extracted callable identity without losing its captured values.
    TermResult<Term> function(const FunctionIdentity &identity);
    // Construct a registered native record with all fields in descriptor order.
    TermResult<Term> native_record(const NativeRecordDescriptor &descriptor, std::span<const Term> fields);

  private:
    // Hide process binding, resource budgets and allocation policy from consumers.
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace erlang_aot::runtime
