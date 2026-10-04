#include "heap_storage.hpp"
#include "off_heap.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <new>

namespace erlang_aot::runtime::detail {
void ChunkDelete::operator()(std::byte *bytes) const noexcept { ::operator delete(bytes); }

HeapStorage::HeapStorage(HeapOptions options, std::weak_ptr<const ContextLifetime> lifetime, AtomStorage &atoms)
    : options(options), lifetime(std::move(lifetime)), atoms(&atoms) {}

HeapStorage::~HeapStorage() { release_off_heap(*this); }

bool HeapStorage::alive() const noexcept {
    const auto token = lifetime.lock();
    return token && token->alive();
}

HeapMark HeapStorage::mark() const noexcept {
    return {chunks.size(), chunks.empty() ? 0 : chunks.back().used, used_words, capacity_words};
}

void HeapStorage::rollback(HeapMark mark) noexcept {
    while (chunks.size() > mark.chunks) {
        chunks.pop_back();
    }
    if (!chunks.empty()) {
        chunks.back().used = mark.tail_words;
    }
    used_words = mark.used_words;
    capacity_words = mark.capacity_words;
    pending = false;
}

namespace {
// Validate arithmetic before reserving backing bytes or changing accounting.
std::expected<void, HeapError> validate(const HeapStorage &storage, std::size_t words, std::size_t alignment) {
    if (words == 0 || words > std::numeric_limits<std::size_t>::max() / sizeof(Word) ||
        !std::has_single_bit(alignment) || alignment < alignof(Word) || alignment > alignof(std::max_align_t)) {
        return std::unexpected(HeapError::invalid_size);
    }
    if (storage.pending) {
        return std::unexpected(HeapError::unsafe_point);
    }
    if (words > storage.options.limit_bytes / sizeof(Word) - storage.used_words - storage.off_heap_words) {
        return std::unexpected(HeapError::limit_exceeded);
    }
    return {};
}

// Fundamental alignment satisfies every accepted reservation alignment; chunks never move on growth.
HeapChunk chunk(std::size_t words) {
    return {std::unique_ptr<std::byte, ChunkDelete>{static_cast<std::byte *>(::operator new(words * sizeof(Word)))},
            words};
}
} // namespace

std::expected<std::span<std::byte>, HeapError> HeapStorage::reserve(std::size_t words, std::size_t alignment) {
    if (const auto valid = validate(*this, words, alignment); !valid) {
        return std::unexpected(valid.error());
    }
    const auto align_words = alignment / sizeof(Word);
    auto offset = chunks.empty() ? 0 : (chunks.back().used + align_words - 1) / align_words * align_words;
    if (chunks.empty() || offset > chunks.back().capacity || words > chunks.back().capacity - offset) {
        const auto remaining = options.limit_bytes / sizeof(Word) - capacity_words - off_heap_words;
        if (words > remaining) {
            return std::unexpected(HeapError::limit_exceeded);
        }
        const auto capacity = std::min(remaining, std::max(words, options.chunk_bytes / sizeof(Word)));
        chunks.push_back(chunk(capacity));
        capacity_words += capacity;
        offset = 0;
    }
    auto &tail = chunks.back();
    // Zero alignment padding as well, so it parses as filler.
    std::memset(tail.bytes.get() + tail.used * sizeof(Word), 0, (offset + words - tail.used) * sizeof(Word));
    used_words += offset + words - tail.used;
    tail.used = offset + words;
    pending = true;
    return std::span<std::byte>{tail.bytes.get() + offset * sizeof(Word), words * sizeof(Word)};
}

std::expected<void, HeapError> HeapStorage::charge(std::size_t words) noexcept {
    if (words > options.limit_bytes / sizeof(Word) - capacity_words - off_heap_words) {
        return std::unexpected(HeapError::limit_exceeded);
    }
    off_heap_words += words;
    return {};
}
} // namespace erlang_aot::runtime::detail
