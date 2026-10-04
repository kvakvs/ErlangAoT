#include <algorithm>
#include <erlang_aot/runtime/process_context.hpp>
#include <limits>
#include <new>

namespace erlang_aot::runtime {
namespace {
// Leave room for the largest inline header of common release allocators (16 bytes on 64-bit Windows heaps).
constexpr std::size_t allocator_overhead = 2 * sizeof(void *);
} // namespace

GeneratedRoots::GeneratedRoots(ProcessContext &owner, RootOptions options) noexcept
    : owner_(owner), options_(options) {}

GeneratedRoots::~GeneratedRoots() {
    while (top_) {
        auto *previous = top_->previous;
        ::operator delete(top_);
        top_ = previous;
    }
}

Word *GeneratedRoots::enter(std::size_t count) noexcept {
    auto &calls = owner_.generated_calls();
    if (!calls.active() || calls.failure()) {
        return nullptr;
    }
    count = std::max(count, std::size_t{1});
    if (frames_.size() >= options_.frames || count > options_.words - std::min(options_.words, words_)) {
        calls.fail_service(abi::v1::Status::resource_limit);
        return nullptr;
    }
    try {
        if (!top_ || top_->capacity - top_->used < count) {
            push_segment(count);
        }
        auto *slots = window(*top_).subspan(top_->used, count).data();
        frames_.push_back(Frame{slots, count, {}});
        top_->used += count;
        words_ += count;
        std::fill_n(slots, count, Word{0});
        return slots;
    } catch (const std::bad_alloc &) {
        release_empty();
        calls.fail_service(abi::v1::Status::out_of_memory);
    }
    return nullptr;
}

std::span<Word> GeneratedRoots::window(Segment &segment) noexcept {
    return {reinterpret_cast<Word *>(&segment + 1), segment.capacity};
}

void GeneratedRoots::push_segment(std::size_t count) {
    if (count > std::numeric_limits<std::size_t>::max() / (2 * sizeof(Word))) {
        throw std::bad_alloc();
    }
    const auto unit = std::max(options_.segment_bytes, allocator_overhead + sizeof(Segment) + sizeof(Word));
    const auto needed = allocator_overhead + sizeof(Segment) + (count * sizeof(Word));
    const auto bytes = ((needed + unit - 1) / unit * unit) - allocator_overhead;
    top_ = ::new (::operator new(bytes)) Segment{top_, (bytes - sizeof(Segment)) / sizeof(Word), 0};
}

void GeneratedRoots::pop() noexcept {
    top_->used -= frames_.back().count;
    words_ -= frames_.back().count;
    frames_.pop_back();
    release_empty();
}

void GeneratedRoots::release_empty() noexcept {
    if (top_ && top_->used == 0) {
        auto *previous = top_->previous;
        ::operator delete(top_);
        top_ = previous;
    }
}

abi::v1::Status GeneratedRoots::leave(Word *frame, Word result) noexcept {
    if (!frame) {
        return abi::v1::Status::ok;
    }
    auto &calls = owner_.generated_calls();
    if (frames_.empty() || frame != frames_.back().slots) {
        calls.fail_service(abi::v1::Status::internal_error);
        return abi::v1::Status::internal_error;
    }
    std::optional<Word> value;
    auto status = abi::v1::Status::ok;
    if (!calls.failure()) {
        if (Term::from_word(result, owner_)) {
            value = result;
        } else {
            status = abi::v1::Status::invalid_argument;
            calls.fail_service(status);
        }
    }
    pop();
    auto &handoff = frames_.empty() ? handoff_ : frames_.back().handoff;
    handoff = value;
    return status;
}

void GeneratedRoots::restore(std::size_t depth) noexcept {
    while (frames_.size() > depth) {
        pop();
    }
    if (depth == 0) {
        handoff_.reset();
    }
}

std::size_t GeneratedRoots::depth() const noexcept { return frames_.size(); }

std::size_t GeneratedRoots::words() const noexcept { return words_; }

std::size_t GeneratedRoots::capacity() const noexcept {
    std::size_t total = 0;
    for (const auto *segment = top_; segment; segment = segment->previous) {
        total += segment->capacity;
    }
    return total;
}

bool GeneratedRoots::contains(Word value) const noexcept {
    if (handoff_ == value) {
        return true;
    }
    for (const auto &frame : frames_) {
        if (frame.handoff == value || std::ranges::contains(std::span(frame.slots, frame.count), value)) {
            return true;
        }
    }
    return false;
}

RootInvocation::RootInvocation(GeneratedRoots &roots) noexcept : roots_(roots), depth_(roots.depth()) {}

RootInvocation::~RootInvocation() { roots_.restore(depth_); }
} // namespace erlang_aot::runtime

erlang_aot::abi::v1::TermWord *erlang_aot_roots_enter_v4(void *context, std::size_t count) noexcept {
    return context ? static_cast<erlang_aot::runtime::ProcessContext *>(context)->roots().enter(count) : nullptr;
}

std::uint8_t erlang_aot_roots_leave_v4(void *context, erlang_aot::abi::v1::TermWord *frame,
                                       erlang_aot::abi::v1::TermWord result) noexcept {
    if (!context) {
        return static_cast<std::uint8_t>(erlang_aot::abi::v1::Status::invalid_argument);
    }
    return static_cast<std::uint8_t>(
        static_cast<erlang_aot::runtime::ProcessContext *>(context)->roots().leave(frame, result));
}
