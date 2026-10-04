#pragma once

// Stable bounded backing storage supports transactional construction; collection remains deferred.
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
    diagnostic_failure,
    expired_context,
    // A heap area failed to parse or a term slot points outside this process's objects.
    corrupt_heap
};

// Report actual collector work; the initial collector stub returns not_implemented instead.
struct CollectionStats final {
    // Measure allocated live/dead term storage before the collection attempt.
    std::size_t bytes_before;
    // Measure retained term storage after tracing/reclamation, excluding unused chunk capacity.
    std::size_t bytes_after;
};

// Summarize a verified heap; counts cover every parsed word of every area.
struct HeapCensus final {
    // Headerless two-word list cells.
    std::size_t cons_cells = 0;
    // Objects that start with a header, excluding filler.
    std::size_t boxed_objects = 0;
    // Unused words skipped by the walker.
    std::size_t filler_words = 0;
    // All parsed words, equal to used_words.
    std::size_t words = 0;
    // Off-heap binary cells on the process's off-heap list.
    std::size_t off_heap_cells = 0;
};

// Bound retained process storage until garbage collection is implemented.
struct HeapOptions final {
    // Minimum backing chunk size; larger individual allocations get a larger chunk.
    std::size_t chunk_bytes = std::size_t{64} * 1024;
    // Maximum total backing capacity, including unused tails and alignment padding.
    std::size_t limit_bytes = std::size_t{64} * 1024 * 1024;
};

namespace detail {
class HeapStorage;
struct IntegerAccess;
struct FloatAccess;
struct MapAccess;
struct BitAccess;
} // namespace detail

struct HeapMark {
    // Restore both retained backing and consumed words when unpublished construction fails.
    std::size_t chunks;
    std::size_t tail_words;
    std::size_t used_words;
    std::size_t capacity_words;
};

class HeapReservation final {
  public:
    // Move the sole rollback obligation while preserving the backing address.
    HeapReservation(HeapReservation &&other) noexcept;
    HeapReservation(const HeapReservation &) = delete;
    HeapReservation &operator=(const HeapReservation &) = delete;
    HeapReservation &operator=(HeapReservation &&) = delete;
    // Roll back unpublished words and newly retained chunks.
    ~HeapReservation();
    // Borrow aligned, zero-initialized storage until commit or rollback.
    std::span<std::byte> bytes() const noexcept;
    // Commit initialized words; an expired owner rolls the reservation back instead.
    std::expected<void, HeapError> commit() noexcept;

  private:
    friend class ProcessHeap;
    // Hold storage through rollback even if the context is removed before this reservation dies.
    HeapReservation(std::shared_ptr<detail::HeapStorage> storage, std::span<std::byte> bytes, HeapMark mark) noexcept;
    // Restore accounting exactly once; committed allocations are retained until storage teardown.
    void rollback() noexcept;
    std::shared_ptr<detail::HeapStorage> storage_;
    std::span<std::byte> bytes_;
    HeapMark mark_;
    bool active_ = true;
};

// Reserve one process's term storage; future growth preserves addresses until an explicit GC safe point.
// Allocation/accounting use target words, while configuration budgets remain exact byte multiples.
class ProcessHeap final {
  public:
    // Release this process's storage owner; reservations and future host pins retain their own ownership.
    ~ProcessHeap();
    // Keep heap identity and all borrowed allocation addresses fixed.
    ProcessHeap(const ProcessHeap &) = delete;
    ProcessHeap &operator=(const ProcessHeap &) = delete;
    ProcessHeap(ProcessHeap &&) = delete;
    ProcessHeap &operator=(ProcessHeap &&) = delete;

    // Commit raw stable words; constructors use reserve so their initialization failures can roll back.
    // Raw words must stay zero (filler) or hold complete objects whenever the heap is walked.
    std::expected<std::span<std::byte>, HeapError> allocate(std::size_t words, DiagnosticSink sink = {}) noexcept;
    // Reserve one unpublished allocation; another allocation requires commit/rollback of the current one.
    std::expected<HeapReservation, HeapError> reserve(std::size_t words,
                                                      std::size_t alignment = alignof(Word)) noexcept;
    // Copy checked owner-independent immediates; rooted graph addition remains deferred.
    TermResult<Term> add(const Term &value) noexcept;
    // Return not_implemented without claiming a safe point or fabricating reclamation statistics.
    std::expected<CollectionStats, HeapError> collect(DiagnosticSink sink = {}) noexcept;
    // Report consumed words (including alignment) and exact retained backing capacity.
    std::size_t used_words() const noexcept;
    std::size_t capacity_words() const noexcept;
    // Report words of off-heap binary buffers created by this process, charged to the same budget.
    std::size_t off_heap_words() const noexcept;
    // Walk every area and check each term slot points at an object of this process (tests, debugging).
    std::expected<HeapCensus, HeapError> verify() const noexcept;

  private:
    friend class ProcessContext;
    friend class TermFactory;
    friend class Term;
    friend struct detail::IntegerAccess;
    friend struct detail::FloatAccess;
    friend struct detail::MapAccess;
    friend struct detail::BitAccess;
    // Bind one process owner and validate heap limits before creating lazy backing storage.
    ProcessHeap(ProcessContext &owner, HeapOptions options);
    // Charge bytes of a new off-heap buffer; failures are reported like reservation failures.
    std::expected<void, HeapError> charge_off_heap(std::size_t bytes) noexcept;
    // Return a charge whose cell was never published.
    void uncharge_off_heap(std::size_t bytes) noexcept;
    // Keep this lazy heap bound to exactly one live process; never transfer it between contexts.
    ProcessContext &owner_;
    // Pin stable backing independently of the context address; liveness still controls admission.
    std::shared_ptr<detail::HeapStorage> storage_;
};
} // namespace erlang_aot::runtime
