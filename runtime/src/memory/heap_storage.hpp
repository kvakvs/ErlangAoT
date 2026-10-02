#pragma once
#include "process_heap.hpp"
#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime::detail {
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

struct HeapResource {
    // Destroy resource-bearing objects before releasing their containing chunks.
    std::byte *bytes;
    HeapDestructor destroy;
};

class HeapStorage final {
  public:
    // Bind validated budgets and a liveness token without allocating backing chunks.
    HeapStorage(HeapOptions options, std::weak_ptr<const ContextLifetime> lifetime);
    // Execute C++ destructors in reverse construction order while all backing bytes still exist.
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

    // Retain bounded backing and explicitly owned destructors independently of future host pins.
    HeapOptions options;
    std::weak_ptr<const ContextLifetime> lifetime;
    std::vector<HeapChunk> chunks;
    std::vector<HeapResource> resources;
    std::size_t used_words = 0;
    std::size_t capacity_words = 0;
    bool pending = false;
};
} // namespace erlang_aot::runtime::detail
