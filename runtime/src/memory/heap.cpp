#include "heap_storage.hpp"
#include <erlang_aot/runtime/process_context.hpp>
#include <new>
#include <stdexcept>

namespace erlang_aot::runtime {
namespace {
// Preserve exact infrastructure statuses; these failures never become Erlang guard rejection.
abi::v1::Status status(HeapError error) {
    using enum HeapError;
    switch (error) {
    case invalid_size:
        return abi::v1::Status::invalid_argument;
    case limit_exceeded:
        return abi::v1::Status::resource_limit;
    case out_of_memory:
        return abi::v1::Status::out_of_memory;
    case unsafe_point:
        return abi::v1::Status::busy;
    case expired_context:
        return abi::v1::Status::stopped;
    case not_implemented:
        return abi::v1::Status::not_implemented;
    case diagnostic_failure:
        return abi::v1::Status::diagnostic_failure;
    case corrupt_heap:
        return abi::v1::Status::internal_error;
    }
    return abi::v1::Status::internal_error;
}
} // namespace

ProcessHeap::ProcessHeap(ProcessContext &owner, HeapOptions options)
    : owner_(owner), storage_(std::make_shared<detail::HeapStorage>(options, owner.lifetime(), owner.atom_storage())) {}

ProcessHeap::~ProcessHeap() = default;

std::expected<HeapReservation, HeapError> ProcessHeap::reserve(std::size_t words, std::size_t alignment) noexcept {
    auto failure = HeapError::out_of_memory;
    try {
        const auto mark = storage_->mark();
        const auto bytes = storage_->reserve(words, alignment);
        if (bytes) {
            return HeapReservation(storage_, *bytes, mark);
        }
        failure = bytes.error();
    } catch (const std::bad_alloc &) {
        failure = HeapError::out_of_memory;
    } catch (const std::length_error &) {
        failure = HeapError::limit_exceeded;
    }
    owner_.generated_calls().fail_service(status(failure));
    return std::unexpected(failure);
}

std::expected<std::span<std::byte>, HeapError> ProcessHeap::allocate(std::size_t words, DiagnosticSink) noexcept {
    auto reservation = reserve(words);
    if (!reservation) {
        return std::unexpected(reservation.error());
    }
    const auto bytes = reservation->bytes();
    const auto committed = reservation->commit();
    if (!committed) {
        owner_.generated_calls().fail_service(status(committed.error()));
        return std::unexpected(committed.error());
    }
    return bytes;
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

std::size_t ProcessHeap::used_words() const noexcept { return storage_->used_words; }

std::size_t ProcessHeap::capacity_words() const noexcept { return storage_->capacity_words; }

std::size_t ProcessHeap::off_heap_words() const noexcept { return storage_->off_heap_words; }

std::expected<void, HeapError> ProcessHeap::charge_off_heap(std::size_t bytes) noexcept {
    const auto charged = storage_->charge((bytes + sizeof(Word) - 1) / sizeof(Word));
    if (!charged) {
        owner_.generated_calls().fail_service(status(charged.error()));
    }
    return charged;
}

void ProcessHeap::uncharge_off_heap(std::size_t bytes) noexcept {
    storage_->off_heap_words -= (bytes + sizeof(Word) - 1) / sizeof(Word);
}
} // namespace erlang_aot::runtime
