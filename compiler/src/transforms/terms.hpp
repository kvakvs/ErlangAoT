#pragma once
// Added for parse transforms: owned Erlang terms exchanged with the host OTP loader (docs/transforms.md).
#include <clause/compiler/lexer.hpp>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace clause::transforms {
// Index of a term inside the Terms arena that created it.
using TermId = std::uint32_t;

enum class TermKind : std::uint8_t { atom, integer, floating, list, tuple, map, bits };

struct TermNode {
    // Kind selects which of the payload fields below is meaningful.
    TermKind kind_ = TermKind::atom;
    // Atom name; canonical decimal digits of an integer; a float.
    std::u32string atom_;
    Integer integer_;
    double float_ = 0;
    // List elements (the tail last when improper_), tuple elements, or map keys and values alternating.
    std::vector<TermId> children_;
    bool improper_ = false;
    // Bitstring bytes, the last one holding its bits in the high positions, and the exact bit count.
    std::string bytes_;
    std::size_t bit_count_ = 0;
};

// A malformed or unsupported term, with a message naming what was wrong and where.
class TermError : public std::runtime_error {
  public:
    explicit TermError(const std::string &message) : std::runtime_error(message) {}
};

// Flat arena of terms: no recursion when terms are destroyed, however deeply they nest.
class Terms {
  public:
    // Create leaves; integers take canonical decimal digits.
    TermId atom(std::u32string_view name);
    TermId integer(Integer value);
    TermId integer(std::int64_t value);
    TermId floating(double value);
    // Create containers from terms of this arena; a list tail that is itself a list is flattened.
    TermId tuple(std::vector<TermId> elements);
    TermId list(std::vector<TermId> elements, std::optional<TermId> tail = std::nullopt);

    TermId nil() { return list({}); }

    // A list of character codes, as Erlang spells strings.
    TermId string(std::u32string_view text);
    // Keys and values alternate: key1, value1, key2, value2, ...
    TermId map(std::vector<TermId> keys_and_values);
    TermId bits(std::string bytes, std::size_t bit_count);

    // Inspect a node; an id past the end throws TermError. Creating terms invalidates earlier references.
    const TermNode &node(TermId id) const;

    std::size_t size() const { return nodes_.size(); }

    // Convenience tests for readers of abstract forms.
    bool is_atom(TermId id, std::u32string_view name) const;
    bool is_nil(TermId id) const;
    // The value of an integer that fits 64 bits, else nothing.
    std::optional<std::int64_t> small_integer(TermId id) const;
    // The characters of a proper list of valid code points, else nothing.
    std::optional<std::u32string> text(TermId id) const;

  private:
    // Every node, addressed by TermId.
    std::vector<TermNode> nodes_;
    // Append one node and return its id.
    TermId add(TermNode node);
};

// The value of canonical decimal digits that fit 64 bits, else nothing.
std::optional<std::int64_t> small_integer(const Integer &value);
// Structural equality of two terms, possibly of different arenas; floats compare by value.
bool equal(const Terms &left, TermId left_id, const Terms &right, TermId right_id);
} // namespace clause::transforms
