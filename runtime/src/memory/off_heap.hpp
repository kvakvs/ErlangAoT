#pragma once
#include "../terms/term_layout.hpp"

// Per-process list of off-heap binary cells (BEAM MSO list); see docs/runtime-heap.md#off-heap-binaries.
namespace erlang_aot::runtime::detail {
class HeapStorage;

// Give a just-published cell its buffer reference and make it the newest entry of the owner's list.
void link_off_heap(HeapStorage &storage, layout::RefcBinaryCell &cell,
                   std::shared_ptr<const layout::BinaryBuffer> buffer) noexcept;
// Move a cell into fresh storage, transferring its buffer reference; the source keeps none.
// List links are the caller's to repair, as a collector sweep rebuilds them.
layout::RefcBinaryCell &relocate_off_heap(layout::RefcBinaryCell &from, std::byte *to) noexcept;
// Drop every reference held by the owner's cells at teardown, newest first.
void release_off_heap(HeapStorage &storage) noexcept;
} // namespace erlang_aot::runtime::detail
