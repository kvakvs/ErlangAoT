#include "heap_storage.hpp"
#include <new>
#include <stdexcept>
#include <utility>

namespace erlang_aot::runtime {
HeapReservation::HeapReservation(std::shared_ptr<detail::HeapStorage> storage, std::span<std::byte> bytes,
                                 HeapMark mark) noexcept
    : storage_(std::move(storage)), bytes_(bytes), mark_(mark) {}

HeapReservation::HeapReservation(HeapReservation &&other) noexcept
    : storage_(std::move(other.storage_)), bytes_(other.bytes_), mark_(other.mark_),
      active_(std::exchange(other.active_, false)) {}

HeapReservation::~HeapReservation() { rollback(); }

void HeapReservation::rollback() noexcept {
    if (active_) {
        storage_->rollback(mark_);
        active_ = false;
    }
}

std::span<std::byte> HeapReservation::bytes() const noexcept {
    return active_ && storage_->alive() ? bytes_ : std::span<std::byte>{};
}

std::expected<void, HeapError> HeapReservation::commit(HeapDestructor destroy) noexcept {
    if (!active_) {
        return std::unexpected(HeapError::invalid_size);
    }
    auto failure = HeapError::expired_context;
    try {
        if (storage_->alive()) {
            if (destroy) {
                storage_->resources.push_back({bytes_.data(), destroy});
            }
            storage_->pending = false;
            active_ = false;
            return {};
        }
    } catch (const std::bad_alloc &) {
        failure = HeapError::out_of_memory;
    } catch (const std::length_error &) {
        failure = HeapError::limit_exceeded;
    }
    if (destroy) {
        destroy(bytes_.data());
    }
    rollback();
    return std::unexpected(failure);
}
} // namespace erlang_aot::runtime
