#include "bounded_stream.hpp"
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace erlang_aot::codegen {
BoundedStream::BoundedStream(const std::size_t limit) : raw_pwrite_stream(true), limit_(limit) {}

void BoundedStream::write_impl(const char *data, const std::size_t size) {
    if (size > std::numeric_limits<std::uint64_t>::max() - position_) {
        failure_ = Failure::limit;
        position_ = std::numeric_limits<std::uint64_t>::max();
        return;
    }
    position_ += size;
    if (failure_ != Failure::none) {
        return;
    }
    if (size > limit_ - bytes_.size()) {
        failure_ = Failure::limit;
        return;
    }
    const auto offset = bytes_.size();
    try {
        bytes_.resize(offset + size);
    } catch (const std::bad_alloc &) {
        failure_ = Failure::allocation;
        return;
    }
    if (size != 0) {
        std::memcpy(bytes_.data() + offset, data, size);
    }
}

void BoundedStream::pwrite_impl(const char *data, const std::size_t size, const std::uint64_t offset) {
    if (failure_ != Failure::none) {
        return;
    }
    if (offset > bytes_.size() || size > bytes_.size() - static_cast<std::size_t>(offset)) {
        failure_ = Failure::patch;
        return;
    }
    if (size != 0) {
        std::memcpy(bytes_.data() + static_cast<std::size_t>(offset), data, size);
    }
}

std::uint64_t BoundedStream::current_pos() const { return position_; }

std::vector<std::byte> BoundedStream::take_bytes() {
    if (failure_ == Failure::allocation) {
        throw std::bad_alloc();
    }
    if (failure_ == Failure::limit) {
        throw std::length_error("compilation artifact byte limit exceeded");
    }
    if (failure_ == Failure::patch) {
        throw std::out_of_range("LLVM artifact patch exceeds serialized bytes");
    }
    return std::move(bytes_);
}
} // namespace erlang_aot::codegen
