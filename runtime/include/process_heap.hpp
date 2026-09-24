#pragma once

// Lazy ownership, checked allocation rejection and immediate copying are implemented; no allocator or GC.
// See docs/runtime-memory.md and runtime/design/processes.md for future roots and collection.
#include <erlang_aot/runtime/features.hpp>
#include <erlang_aot/runtime/terms.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace erlang_aot::runtime {
// Distinguish invalid requests, configured limits and backing allocation failures.
enum class HeapError : std::uint8_t {
    invalid_size,
    limit_exceeded,
    out_of_memory,
    unsafe_point,
    not_implemented,
    diagnostic_failure
};

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

// Reserve one process's term storage; future growth preserves addresses until an explicit GC safe point.
// Allocation/accounting use target words, while configuration budgets remain exact byte multiples.
class ProcessHeap final {
  public:
    // The lazy boundary owns no backing storage; future chunk/resource owners must clean up on exit.
    ~ProcessHeap() = default;
    // Keep heap identity and all borrowed allocation addresses fixed.
    ProcessHeap(const ProcessHeap &) = delete;
    ProcessHeap &operator=(const ProcessHeap &) = delete;
    ProcessHeap(ProcessHeap &&) = delete;
    ProcessHeap &operator=(ProcessHeap &&) = delete;

    // Validate nonzero word count, byte overflow and budget; valid requests return not_implemented.
    std::expected<std::span<std::byte>, HeapError> allocate(std::size_t words, DiagnosticSink sink = {}) noexcept;
    // Copy checked owner-independent immediates; rooted graph addition remains deferred.
    TermResult<Term> add(const Term &value) noexcept;
    // Return not_implemented without claiming a safe point or fabricating reclamation statistics.
    std::expected<CollectionStats, HeapError> collect(DiagnosticSink sink = {}) noexcept;
    // Report allocated and retained capacity in words; both stay zero until allocation is implemented.
    std::size_t used_words() const noexcept;
    std::size_t capacity_words() const noexcept;

  private:
    friend class ProcessContext;
    friend class TermFactory;
    friend class Term;
    // Bind one process owner and validate heap limits before creating lazy backing storage.
    ProcessHeap(ProcessContext &owner, HeapOptions options);
    // Keep allocation policy independent of term layout or scheduler priority.
    HeapOptions options_;
    // Keep this lazy heap bound to exactly one live process; never transfer it between contexts.
    ProcessContext &owner_;
    // Future storage must own stable chunks and destroy C++ resources before freeing their backing memory.
    // Host, continuation, mailbox and cursor roots require a separate registry before heap Terms are admitted.
    // Track checked allocation/capacity totals without rescanning chunks.
    std::size_t used_words_ = 0;
    std::size_t capacity_words_ = 0;
};
} // namespace erlang_aot::runtime
