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
    expired_context
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

namespace detail {
class HeapStorage;
struct IntegerAccess;
struct FloatAccess;
struct MapAccess;
} // namespace detail

struct HeapMark {
    // Restore both retained backing and consumed words when unpublished construction fails.
    std::size_t chunks;
    std::size_t tail_words;
    std::size_t used_words;
    std::size_t capacity_words;
};

// Transfer destruction only after a resource has been constructed in its reserved storage.
using HeapDestructor = void (*)(std::byte *) noexcept;

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
    // Commit initialized bytes; on failure destroy the supplied resource before rolling back.
    std::expected<void, HeapError> commit(HeapDestructor destroy = nullptr) noexcept;

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

  private:
    friend class ProcessContext;
    friend class TermFactory;
    friend class Term;
    friend struct detail::IntegerAccess;
    friend struct detail::FloatAccess;
    friend struct detail::MapAccess;
    // Bind one process owner and validate heap limits before creating lazy backing storage.
    ProcessHeap(ProcessContext &owner, HeapOptions options);
    // Keep this lazy heap bound to exactly one live process; never transfer it between contexts.
    ProcessContext &owner_;
    // Pin stable backing independently of the context address; liveness still controls admission.
    std::shared_ptr<detail::HeapStorage> storage_;
};
} // namespace erlang_aot::runtime
