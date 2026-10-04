#include "heap_storage.hpp"
#include "off_heap.hpp"
#include <algorithm>
#include <limits>
#include <memory>
#include <new>

namespace erlang_aot::runtime::detail {
std::span<const Word> HeapArea::used() const noexcept { return {words_.get(), top_}; }

std::span<const Word> HeapArea::from(std::uintptr_t address) const noexcept {
    const auto begin = reinterpret_cast<std::uintptr_t>(words_.get());
    if (address < begin || (address - begin) % sizeof(Word) != 0 || (address - begin) / sizeof(Word) >= top_) {
        return {};
    }
    return used().subspan((address - begin) / sizeof(Word));
}

bool HeapArea::fits(std::size_t words) const noexcept { return words <= capacity_ - top_; }

std::span<Word> HeapArea::bump(std::size_t words) noexcept {
    const std::span<Word> reserved{words_.get() + top_, words};
    std::ranges::fill(reserved, Word{0});
    top_ += words;
    return reserved;
}

HeapStorage::HeapStorage(HeapOptions options, std::weak_ptr<const ContextLifetime> lifetime, AtomStorage &atoms)
    : options_(options), lifetime_(std::move(lifetime)), atoms_(&atoms) {}

HeapStorage::~HeapStorage() { release_off_heap(*this); }

bool HeapStorage::alive() const noexcept {
    const auto token = lifetime_.lock();
    return token && token->alive();
}

HeapMark HeapStorage::mark() const noexcept {
    return {heap_.capacity_, heap_.top_,     fragments_.size(), fragments_.empty() ? 0 : fragments_.back().top_,
            used_words_,     capacity_words_};
}

void HeapStorage::rollback(HeapMark mark) noexcept {
    while (fragments_.size() > mark.fragments) {
        fragments_.pop_back();
    }
    std::erase_if(ranges_, [&](const FragmentRange &range) { return range.fragment_ >= fragments_.size(); });
    if (!fragments_.empty()) {
        fragments_.back().top_ = mark.fragment_top;
    }
    if (mark.heap_capacity == 0) {
        heap_ = {};
    } else {
        heap_.top_ = mark.heap_top;
    }
    used_words_ = mark.used_words;
    capacity_words_ = mark.capacity_words;
    pending_ = false;
}

namespace {
// Validate arithmetic before reserving backing words or changing accounting.
std::expected<void, HeapError> validate(const HeapStorage &storage, std::size_t words) {
    if (words == 0 || words > std::numeric_limits<std::size_t>::max() / sizeof(Word)) {
        return std::unexpected(HeapError::invalid_size);
    }
    if (storage.pending_) {
        return std::unexpected(HeapError::unsafe_point);
    }
    if (words > storage.options_.limit_bytes / sizeof(Word) - storage.used_words_ - storage.off_heap_words_) {
        return std::unexpected(HeapError::limit_exceeded);
    }
    return {};
}
} // namespace

std::span<const Word> HeapStorage::owned(std::uintptr_t address) const noexcept {
    if (const auto found = heap_.from(address); !found.empty()) {
        return found;
    }
    const auto range = std::ranges::upper_bound(ranges_, address, {}, &FragmentRange::begin_);
    return range == ranges_.begin() ? std::span<const Word>{} : fragments_[std::prev(range)->fragment_].from(address);
}

std::expected<HeapArea, HeapError> HeapStorage::block(std::size_t words) const {
    const auto remaining = options_.limit_bytes / sizeof(Word) - capacity_words_ - off_heap_words_;
    if (words > remaining) {
        return std::unexpected(HeapError::limit_exceeded);
    }
    const auto capacity = std::min(remaining, std::max(words, options_.min_heap_words));
    return HeapArea{std::make_unique_for_overwrite<Word[]>(capacity), capacity};
}

std::expected<HeapArea *, HeapError> HeapStorage::add_fragment(std::size_t words) {
    // Grow the chain geometrically up front, so the insertions below cannot throw.
    if (fragments_.size() == fragments_.capacity()) {
        fragments_.reserve(2 * fragments_.size() + 1);
    }
    ranges_.reserve(fragments_.capacity());
    auto fresh = block(words);
    if (!fresh) {
        return std::unexpected(fresh.error());
    }
    const auto begin = reinterpret_cast<std::uintptr_t>(fresh->words_.get());
    ranges_.insert(std::ranges::upper_bound(ranges_, begin, {}, &FragmentRange::begin_), {begin, fragments_.size()});
    capacity_words_ += fresh->capacity_;
    return &fragments_.emplace_back(std::move(*fresh));
}

std::expected<HeapArea *, HeapError> HeapStorage::area_for(std::size_t words) {
    if (heap_.capacity_ == 0) {
        auto fresh = block(words);
        if (!fresh) {
            return std::unexpected(fresh.error());
        }
        heap_ = std::move(*fresh);
        capacity_words_ += heap_.capacity_;
    }
    if (heap_.fits(words)) {
        return &heap_;
    }
    if (!fragments_.empty() && fragments_.back().fits(words)) {
        return &fragments_.back();
    }
    return add_fragment(words);
}

std::expected<std::span<Word>, HeapError> HeapStorage::reserve(std::size_t words) {
    if (const auto valid = validate(*this, words); !valid) {
        return std::unexpected(valid.error());
    }
    // A failed or throwing area_for changed nothing: a new heap block always fits the request.
    const auto area = area_for(words);
    if (!area) {
        return std::unexpected(area.error());
    }
    used_words_ += words;
    pending_ = true;
    return (*area)->bump(words);
}

std::expected<void, HeapError> HeapStorage::charge(std::size_t words) noexcept {
    if (words > options_.limit_bytes / sizeof(Word) - capacity_words_ - off_heap_words_) {
        return std::unexpected(HeapError::limit_exceeded);
    }
    off_heap_words_ += words;
    return {};
}
} // namespace erlang_aot::runtime::detail
