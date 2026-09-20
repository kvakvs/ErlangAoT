#pragma once

// REVIEW SKETCH ONLY: declarations without definitions, excluded from the build.
// See processes.md for allocation, ownership and future collection boundaries.
#include "terms.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <vector>

namespace erlang_aot::runtime {
// Distinguish invalid requests, configured limits and backing allocation failures.
enum class HeapError : std::uint8_t { invalid_size, limit_exceeded, out_of_memory, unsafe_point, not_implemented };

// Report actual collector work; the initial collector stub returns not_implemented instead.
struct CollectionStats final {
    // Measure allocated live/dead term storage before the collection attempt.
    std::size_t bytes_before;
    // Measure retained term storage after tracing/reclamation, excluding unused chunk capacity.
    std::size_t bytes_after;
};

// Bound retained process storage until garbage collection is implemented.
struct HeapOptions final {
    // Minimum backing chunk size; larger individual allocations get a larger chunk.
    std::size_t chunk_bytes = std::size_t{64} * 1024;
    // Maximum total backing capacity, including unused tails and alignment padding.
    std::size_t limit_bytes = std::size_t{64} * 1024 * 1024;
};

// Own process term memory and roots; growth preserves addresses, future collection may move cells.
// A new process is born with a heap set to a default value (configurable via stdlib Erlang call)
// A heap can grow as necessary without limitation, and can shrink via garbage collection
// Heap storage sizes are measured in Words (machine word width of target architecture)
class ProcessHeap final {
  public:
    // Release every chunk when the owning process is reaped.
    ~ProcessHeap();
    // Keep heap identity and all borrowed allocation addresses fixed.
    ProcessHeap(const ProcessHeap &) = delete;
    ProcessHeap &operator=(const ProcessHeap &) = delete;
    ProcessHeap(ProcessHeap &&) = delete;
    ProcessHeap &operator=(ProcessHeap &&) = delete;

    // Allocate nonzero bytes rounded to target words; no existing allocation moves.
    std::expected<std::span<std::byte>, HeapError> allocate(std::size_t words);
    // Add an independently owned term graph; equivalent to value.copy_to(*this).
    TermResult<Term> add(const Term &value);
    // Trace registered roots at a safe point; TODO(gc): initially report not_implemented.
    std::expected<CollectionStats, HeapError> collect();
    // Report rounded allocated bytes and total retained backing capacity, respectively.
    std::size_t used_words() const noexcept;
    std::size_t capacity_words() const noexcept;

  private:
    friend class ProcessContext;
    friend class TermFactory;
    friend class Term;
    // Bind one process owner and validate heap limits before creating lazy backing storage.
    ProcessHeap(ProcessContext &owner, HeapOptions options);
    // Back each chunk with target words; vector growth moves owners, never word arrays.
    struct Chunk;
    // Keep allocation policy independent of term layout or scheduler priority.
    HeapOptions options_;
    // Retain all chunks until process exit; there is no individual deallocation yet.
    std::vector<std::unique_ptr<Chunk>> chunks_;
    // Track host handles, mailbox and continuation roots plus owner/safe-point validation.
    class Roots;
    std::unique_ptr<Roots> roots_;
    // Track checked allocation/capacity totals without rescanning chunks.
    std::size_t used_words_ = 0;
    std::size_t capacity_words_ = 0;

    // Append a checked-size chunk after the collection placeholder declines to reclaim.
    std::expected<void, HeapError> grow_by(std::size_t words_to_append);
    // TODO(gc): ignore collect()'s initial not_implemented result and grow without reclamation.
    void collection_placeholder(std::size_t requested_words) noexcept;
};
} // namespace erlang_aot::runtime
