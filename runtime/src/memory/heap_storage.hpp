#pragma once
#include "heap_object.hpp"
#include "process_heap.hpp"
#include <cstdint>
#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime::detail {
namespace layout {
struct RefcBinaryCell;
} // namespace layout

struct ChunkDelete {
    // Pair fundamental-alignment allocation with its matching C++ deallocator.
    void operator()(std::byte *bytes) const noexcept;
};

struct HeapChunk {
    // Stable backing never moves when the owning vector grows.
    std::unique_ptr<std::byte, ChunkDelete> bytes;
    std::size_t capacity;
    std::size_t used = 0;
    // One bit per word, set at every published object start (header or cons cell).
    std::vector<std::uint64_t> starts;

    // Borrow the used words as a walkable area.
    std::span<const Word> area() const noexcept;
    // Report whether a published object starts at this word offset.
    bool started(std::size_t word) const noexcept;
    // Record a published object start at this word offset.
    void mark(std::size_t word) noexcept;
};

struct ChunkRange {
    // First byte address of a chunk and its index in HeapStorage::chunks, for address lookups.
    std::uintptr_t begin;
    std::size_t chunk;
};

class HeapStorage final {
  public:
    // Bind validated budgets and a liveness token without allocating backing chunks.
    HeapStorage(HeapOptions options, std::weak_ptr<const ContextLifetime> lifetime, AtomStorage &atoms);
    // Release every off-heap reference while all backing bytes still exist.
    ~HeapStorage();
    HeapStorage(const HeapStorage &) = delete;
    HeapStorage &operator=(const HeapStorage &) = delete;
    // Check lifetime before admitting a surviving reservation or future heap Term.
    bool alive() const noexcept;
    // Capture exact accounting before an unpublished reservation changes the tail.
    HeapMark mark() const noexcept;
    // Restore a reservation without touching any previously published bytes.
    void rollback(HeapMark mark) noexcept;
    // Grow stable backing only after validating extent, alignment and remaining capacity.
    std::expected<std::span<std::byte>, HeapError> reserve(std::size_t words, std::size_t alignment);
    // Count a newly created off-heap buffer against the budget shared with heap backing.
    std::expected<void, HeapError> charge(std::size_t words) noexcept;
    // Borrow the used words from a published object start to the end of its chunk; empty unless the
    // address is word-aligned below a chunk's used words and an object was published there.
    std::span<const Word> published(std::uintptr_t address) const noexcept;
    // Mark every object start in freshly committed words of the newest chunk.
    void mark_published(std::span<const std::byte> bytes) noexcept;

    // Retain bounded backing independently of future host pins.
    HeapOptions options;
    std::weak_ptr<const ContextLifetime> lifetime;
    std::vector<HeapChunk> chunks;
    // Chunk ranges sorted by address; rollback drops entries of removed chunks.
    std::vector<ChunkRange> ranges;
    std::size_t used_words = 0;
    std::size_t capacity_words = 0;
    // Words of off-heap buffers created by this process; capacity_words + off_heap_words stays within budget.
    std::size_t off_heap_words = 0;
    // Head of this process's off-heap binary cells, newest first; the only route to their C++ state.
    layout::RefcBinaryCell *off_heap = nullptr;
    bool pending = false;
    // Borrow the runtime atom table only while the process lifetime token remains alive.
    AtomStorage *atoms;
};

// Commit fully initialized objects, mark their starts and admit value; failure rolls the reservation back.
TermResult<Term> publish(const std::shared_ptr<HeapStorage> &storage, HeapReservation &reservation,
                         Word value) noexcept;
} // namespace erlang_aot::runtime::detail
