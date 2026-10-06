#pragma once
#include "process_heap.hpp"
#include <cstddef>

namespace erlang_aot::runtime::detail {
// Words of all process memory of one runtime (heap blocks, fragments, off-heap buffers and stacks) and its optional
// limit (docs/runtime-heap.md#runtime-memory-limit). Calls are host-serialized like the rest of the runtime.
class RuntimeMemory final {
  public:
    // Bind the limit; the unlimited default is larger than any address space.
    explicit RuntimeMemory(std::size_t limit_words) noexcept : limit_words_(limit_words) {}

    // Words that can still be charged under the limit; none while a collection's to-space is past it. Without a
    // limit every request fits, so only the process budget or the host can refuse it.
    std::size_t available() const noexcept {
        if (limit_words_ == UNLIMITED_WORDS) {
            return UNLIMITED_WORDS;
        }
        return used_words_ < limit_words_ ? limit_words_ - used_words_ : 0;
    }

    // Charge words that fit under the limit; false changes nothing.
    bool charge(std::size_t words) noexcept {
        if (words > available()) {
            return false;
        }
        used_words_ += words;
        return true;
    }

    // Charge words even past the limit: storage already bounded by available(), or a collection's to-space,
    // which replaces the blocks it releases.
    void force(std::size_t words) noexcept { used_words_ += words; }

    // Return words charged earlier.
    void release(std::size_t words) noexcept { used_words_ -= words; }

    // Words charged by every process of the runtime.
    std::size_t used() const noexcept { return used_words_; }

  private:
    // The limit of the uncapped default, RuntimeOptions::memory_limit_bytes = UNLIMITED_HEAP_BYTES.
    static constexpr std::size_t UNLIMITED_WORDS = UNLIMITED_HEAP_BYTES / sizeof(Word);
    // Cap on used_words_ for new charges.
    std::size_t limit_words_;
    // Words charged and not yet released.
    std::size_t used_words_ = 0;
};
} // namespace erlang_aot::runtime::detail
