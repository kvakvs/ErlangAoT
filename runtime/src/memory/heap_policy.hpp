#pragma once
#include "process_heap.hpp"

namespace erlang_aot::runtime::detail {
// Reject a fractional word budget or a minimum heap above it before publishing a process owner.
inline bool valid_heap_options(HeapOptions options) noexcept {
    return options.min_heap_words != 0 && options.limit_bytes % sizeof(Word) == 0 &&
           options.min_heap_words <= options.limit_bytes / sizeof(Word);
}
} // namespace erlang_aot::runtime::detail
