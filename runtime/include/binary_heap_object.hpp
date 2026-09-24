#pragma once

// REVIEW SKETCH ONLY: declarations without allocation or reclamation implementations.
#include "base_types.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace erlang_aot::runtime {
// Distinguish invalid input from size overflow and vector/control-block allocation failure.
enum class BinaryHeapObjectError : std::uint8_t {
    // Refcounted storage requires words.size() > HEAP_BINARY_THRESHOLD_WORDS; includes rejecting empty input.
    invalid_size,
    // A supplied tail count must be in 0..ERL_WORD_BITS-1.
    invalid_trailing_bits,
    size_overflow,
    out_of_memory
};

// Share immutable, stable-address binary data across processes through std::shared_ptr.
// Words occur in sequence; each word's most significant bit comes first, independent of host byte order.
class BinaryHeapObject final : public std::enable_shared_from_this<BinaryHeapObject> {
  public:
    using Ptr = std::shared_ptr<BinaryHeapObject>;
    // Validate word count and tail, check bit/byte sizes, then copy into a vector and zero unused low bits.
    // Nullopt means all supplied words are full; roll back on failure without publishing an object.
    static std::expected<Ptr, BinaryHeapObjectError> create(std::span<const Word> words, Word trailing_word_bits = 0);
    // TODO in create(): std::copy(values.begin(), values.end(), values_);

    // Release the owned vector when the last shared object owner disappears.
    ~BinaryHeapObject() = default;
    // Preserve object identity and borrowed word addresses; share ownership instead of copying or moving.
    BinaryHeapObject(const BinaryHeapObject &) = delete;
    BinaryHeapObject &operator=(const BinaryHeapObject &) = delete;
    BinaryHeapObject(BinaryHeapObject &&) = delete;
    BinaryHeapObject &operator=(BinaryHeapObject &&) = delete;

    // Borrow more than HEAP_BINARY_THRESHOLD_WORDS immutable words; retain a shared owner while using them.
    std::span<const Word> words() const noexcept;
    // Return valid high bits in a partial last word (1..ERL_WORD_BITS-1); nullopt means no partial word.
    Word trailing_word_bits() const noexcept;
    // Return the exact logical length in bits, excluding zeroed low padding bits; always greater than 512.
    std::size_t bit_size() const noexcept;
    // Identify byte-aligned bit lengths, including partial words containing whole bytes.
    bool is_binary() const noexcept;

  private:
    // Move validated, normalized vector storage into an object before create() publishes its shared_ptr.
    BinaryHeapObject(std::vector<Word> words, std::optional<Word> trailing_word_bits) noexcept;

    // Own contiguous words directly; no resizing or mutation is allowed after publication.
    std::vector<Word> words_;
    // Record valid bits in the final word only when that word is partial; 0 when all words are full.
    Word trailing_word_bits_;
};
} // namespace erlang_aot::runtime
