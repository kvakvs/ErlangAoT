#pragma once
#include "process_heap.hpp"

namespace clause::runtime::detail {
// Off-heap words that make a safepoint collect before any collection ran (ERTS bin_vheap_sz default).
inline constexpr std::size_t MIN_BINARY_HEAP_WORDS = 46'422;

// Reject a fractional word budget or a minimum heap above it before publishing a process owner.
inline bool valid_heap_options(HeapOptions options) noexcept {
    return options.min_heap_words != 0 && options.limit_bytes % sizeof(Word) == 0 &&
           options.min_heap_words <= options.limit_bytes / sizeof(Word);
}
} // namespace clause::runtime::detail
