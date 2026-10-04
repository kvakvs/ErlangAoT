#pragma once
#include "heap_object.hpp"
#include "process_heap.hpp"
#include <erlang_aot/runtime/process_context.hpp>
#include <map>

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

    // Retain bounded backing independently of future host pins.
    HeapOptions options;
    std::weak_ptr<const ContextLifetime> lifetime;
    std::vector<HeapChunk> chunks;
    std::size_t used_words = 0;
    std::size_t capacity_words = 0;
    // Words of off-heap buffers created by this process; capacity_words + off_heap_words stays within budget.
    std::size_t off_heap_words = 0;
    // Head of this process's off-heap binary cells, newest first; the only route to their C++ state.
    layout::RefcBinaryCell *off_heap = nullptr;
    bool pending = false;
    // Own exact published starts; object entries remain stable while host Terms pin this storage.
    std::map<Word, HeapObject> objects;
    // Borrow the runtime atom table only while the process lifetime token remains alive.
    AtomStorage *atoms;
};

// Publish a fully initialized object batch atomically; constructor-owned reservations roll back on failure.
TermResult<Term> publish(const std::shared_ptr<HeapStorage> &storage, HeapReservation &reservation,
                         std::span<const HeapObject> objects) noexcept;
} // namespace erlang_aot::runtime::detail
