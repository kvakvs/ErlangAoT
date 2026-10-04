#include <algorithm>
#include <erlang_aot/runtime/process_context.hpp>
#include <memory>
#include <new>

namespace erlang_aot::runtime {
GeneratedRoots::GeneratedRoots(ProcessContext &owner, RootOptions options) noexcept
    : owner_(owner), options_(options) {}

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
        if (segments_.empty() || segments_.back().capacity - segments_.back().used < count) {
            const auto capacity = std::max(count, options_.segment_words);
            segments_.push_back(Segment{std::make_unique_for_overwrite<Word[]>(capacity), capacity, 0});
        }
        auto &segment = segments_.back();
        auto *slots = segment.words.get() + segment.used;
        frames_.push_back(Frame{slots, count, {}});
        segment.used += count;
        words_ += count;
        std::fill_n(slots, count, Word{0});
        return slots;
    } catch (const std::bad_alloc &) {
        release_empty();
        calls.fail_service(abi::v1::Status::out_of_memory);
    }
    return nullptr;
}

void GeneratedRoots::pop() noexcept {
    segments_.back().used -= frames_.back().count;
    words_ -= frames_.back().count;
    frames_.pop_back();
    release_empty();
}

void GeneratedRoots::release_empty() noexcept {
    if (!segments_.empty() && segments_.back().used == 0) {
        segments_.pop_back();
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
    for (const auto &segment : segments_) {
        total += segment.capacity;
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
