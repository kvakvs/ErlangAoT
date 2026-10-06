#include "off_heap.hpp"
#include "heap_storage.hpp"
#include "heap_walk.hpp"
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

namespace {
// Destroy a dead cell's reference, returning the buffer's charge when no other cell still shares it.
void release(HeapStorage &storage, layout::RefcBinaryCell &cell) noexcept {
    if (cell.buffer_.use_count() == 1) {
        storage.uncharge((cell.buffer_->size() + sizeof(Word) - 1) / sizeof(Word));
    }
    std::destroy_at(&cell);
}
} // namespace

void sweep_off_heap(HeapStorage &storage, HeapArea &copies) noexcept {
    layout::RefcBinaryCell *newest = nullptr;
    auto **link = &newest;
    for (auto *cell = std::exchange(storage.off_heap_, nullptr); cell != nullptr;) {
        auto *next = cell->next_;
        const auto first = cell->header_.value_;
        if (is_header(first)) {
            release(storage, *cell);
        } else {
            // A forwarded cell's first word is the boxed pointer to its copy.
            const auto copy =
                copies.from(static_cast<std::uintptr_t>(first & ~static_cast<Word>(abi::v1::primary_mask)));
            *link = reinterpret_cast<layout::RefcBinaryCell *>(copy.data());
            link = &(*link)->next_;
        }
        cell = next;
    }
    *link = nullptr;
    storage.off_heap_ = newest;
}

void release_off_heap(HeapStorage &storage) noexcept {
    for (auto *cell = std::exchange(storage.off_heap_, nullptr); cell != nullptr;) {
        auto *next = cell->next_;
        std::destroy_at(cell);
        cell = next;
    }
}
} // namespace erlang_aot::runtime::detail
