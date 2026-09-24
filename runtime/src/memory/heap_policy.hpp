#pragma once
#include "process_heap.hpp"

namespace erlang_aot::runtime::detail {
// Reject fractional target words or inconsistent budgets before publishing a process owner.
inline bool valid_heap_options(HeapOptions options) noexcept {
    return options.chunk_bytes != 0 && options.chunk_bytes <= options.limit_bytes &&
           options.chunk_bytes % sizeof(Word) == 0 && options.limit_bytes % sizeof(Word) == 0;
}
} // namespace erlang_aot::runtime::detail
