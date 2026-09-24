#include "process_heap.hpp"
#include <limits>

namespace erlang_aot::runtime {
ProcessHeap::ProcessHeap(ProcessContext &owner, HeapOptions options) : options_(options), owner_(owner) {}

std::expected<std::span<std::byte>, HeapError> ProcessHeap::allocate(std::size_t words, DiagnosticSink sink) noexcept {
    if (words == 0 || words > std::numeric_limits<std::size_t>::max() / sizeof(Word)) {
        return std::unexpected(HeapError::invalid_size);
    }
    if (words > options_.limit_bytes / sizeof(Word)) {
        return std::unexpected(HeapError::limit_exceeded);
    }
    return deferred_service<HeapError>(abi::v1::FeatureId::allocation, "ProcessHeap::allocate", sink);
}

std::expected<CollectionStats, HeapError> ProcessHeap::collect(DiagnosticSink sink) noexcept {
    return deferred_service<HeapError>(abi::v1::FeatureId::garbage_collection, "ProcessHeap::collect", sink);
}

std::size_t ProcessHeap::used_words() const noexcept { return used_words_; }

std::size_t ProcessHeap::capacity_words() const noexcept { return capacity_words_; }
} // namespace erlang_aot::runtime
