#include "process_heap.hpp"
#include <erlang_aot/runtime/process_context.hpp>
#include <limits>

namespace erlang_aot::runtime {
ProcessHeap::ProcessHeap(ProcessContext &owner, HeapOptions options) : options_(options), owner_(owner) {}

std::expected<std::span<std::byte>, HeapError> ProcessHeap::allocate(std::size_t words, DiagnosticSink sink) noexcept {
    if (words == 0 || words > std::numeric_limits<std::size_t>::max() / sizeof(Word)) {
        owner_.generated_calls().fail_service(abi::v1::Status::invalid_argument);
        return std::unexpected(HeapError::invalid_size);
    }
    if (words > options_.limit_bytes / sizeof(Word)) {
        owner_.generated_calls().fail_service(abi::v1::Status::resource_limit);
        return std::unexpected(HeapError::limit_exceeded);
    }
    const auto failure = deferred_service<HeapError>(abi::v1::FeatureId::allocation, "ProcessHeap::allocate", sink);
    owner_.generated_calls().fail_service(failure.error() == HeapError::not_implemented
                                              ? abi::v1::Status::not_implemented
                                              : abi::v1::Status::diagnostic_failure,
                                          true);
    return failure;
}

std::expected<CollectionStats, HeapError> ProcessHeap::collect(DiagnosticSink sink) noexcept {
    const auto failure =
        deferred_service<HeapError>(abi::v1::FeatureId::garbage_collection, "ProcessHeap::collect", sink);
    owner_.generated_calls().fail_service(failure.error() == HeapError::not_implemented
                                              ? abi::v1::Status::not_implemented
                                              : abi::v1::Status::diagnostic_failure,
                                          true);
    return failure;
}

std::size_t ProcessHeap::used_words() const noexcept { return used_words_; }

std::size_t ProcessHeap::capacity_words() const noexcept { return capacity_words_; }
} // namespace erlang_aot::runtime
