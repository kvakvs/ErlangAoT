#include "heap_storage.hpp"
#include "heap_walk.hpp"
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
    std::erase_if(ranges, [&](const ChunkRange &range) { return range.chunk >= chunks.size(); });
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
    std::vector<std::uint64_t> starts((words + 63) / 64);
    return {std::unique_ptr<std::byte, ChunkDelete>{static_cast<std::byte *>(::operator new(words * sizeof(Word)))},
            words, 0, std::move(starts)};
}
} // namespace

std::span<const Word> HeapChunk::area() const noexcept { return {reinterpret_cast<const Word *>(bytes.get()), used}; }

bool HeapChunk::started(std::size_t word) const noexcept { return ((starts[word / 64] >> (word % 64)) & 1U) != 0; }

void HeapChunk::mark(std::size_t word) noexcept { starts[word / 64] |= std::uint64_t{1} << (word % 64); }

std::span<const Word> HeapStorage::published(std::uintptr_t address) const noexcept {
    const auto range = std::ranges::upper_bound(ranges, address, {}, &ChunkRange::begin);
    if (range == ranges.begin()) {
        return {};
    }
    const auto &found = chunks[std::prev(range)->chunk];
    const auto offset = address - std::prev(range)->begin;
    const auto word = offset / sizeof(Word);
    if (offset % sizeof(Word) != 0 || word >= found.used || !found.started(word)) {
        return {};
    }
    return found.area().subspan(word);
}

void HeapStorage::mark_published(std::span<const std::byte> bytes) noexcept {
    auto &tail = chunks.back();
    const auto *base = reinterpret_cast<const Word *>(tail.bytes.get());
    std::span rest{reinterpret_cast<const Word *>(bytes.data()), bytes.size() / sizeof(Word)};
    while (!rest.empty()) {
        const auto cell = parse_cell(rest);
        // Factory-written objects always parse; a malformed tail is left unmarked and so never admitted.
        if (!cell) {
            return;
        }
        if (cell->shape != HeapCell::Shape::filler) {
            tail.mark(static_cast<std::size_t>(cell->words.data() - base));
        }
        rest = rest.subspan(cell->words.size());
    }
}

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
        auto fresh = chunk(capacity);
        chunks.reserve(chunks.size() + 1);
        ranges.reserve(ranges.size() + 1);
        const auto begin = reinterpret_cast<std::uintptr_t>(fresh.bytes.get());
        ranges.insert(std::ranges::upper_bound(ranges, begin, {}, &ChunkRange::begin), {begin, chunks.size()});
        chunks.push_back(std::move(fresh));
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
