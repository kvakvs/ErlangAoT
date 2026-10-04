#include "off_heap.hpp"
#include "heap_storage.hpp"
#include <memory>
#include <utility>

namespace erlang_aot::runtime::detail {
void link_off_heap(HeapStorage &storage, layout::RefcBinaryCell &cell,
                   std::shared_ptr<const layout::BinaryBuffer> buffer) noexcept {
    cell.buffer_ = std::move(buffer);
    cell.next_ = storage.off_heap_;
    storage.off_heap_ = &cell;
}

layout::RefcBinaryCell &relocate_off_heap(layout::RefcBinaryCell &from, std::byte *to) noexcept {
    return *std::construct_at(reinterpret_cast<layout::RefcBinaryCell *>(to), std::move(from));
}

void release_off_heap(HeapStorage &storage) noexcept {
    for (auto *cell = std::exchange(storage.off_heap_, nullptr); cell != nullptr;) {
        auto *next = cell->next_;
        std::destroy_at(cell);
        cell = next;
    }
}
} // namespace erlang_aot::runtime::detail
