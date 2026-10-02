#include <algorithm>
#include <erlang_aot/runtime/process_context.hpp>
#include <limits>
#include <new>
#include <stdexcept>

namespace erlang_aot::runtime {
GeneratedRoots::GeneratedRoots(ProcessContext &owner, RootOptions options) noexcept
    : owner_(owner), options_(options) {}

Word *GeneratedRoots::enter(std::size_t count) noexcept {
    auto &calls = owner_.generated_calls();
    if (!calls.active() || calls.failure()) {
        return nullptr;
    }
    count = std::max(count, std::size_t{1});
    if (frames_.size() >= options_.frames || count > options_.words - std::min(options_.words, words_) ||
        count > std::numeric_limits<std::size_t>::max() / sizeof(Word)) {
        calls.fail_service(abi::v1::Status::resource_limit);
        return nullptr;
    }
    try {
        Frame frame{std::make_unique<Word[]>(count), count, {}};
        frames_.push_back(std::move(frame));
        words_ += count;
        return frames_.back().slots.get();
    } catch (const std::bad_alloc &) {
        calls.fail_service(abi::v1::Status::out_of_memory);
    } catch (const std::length_error &) {
        calls.fail_service(abi::v1::Status::resource_limit);
    }
    return nullptr;
}

abi::v1::Status GeneratedRoots::leave(Word *frame, Word result) noexcept {
    if (!frame) {
        return abi::v1::Status::ok;
    }
    auto &calls = owner_.generated_calls();
    if (frames_.empty() || frame != frames_.back().slots.get()) {
        calls.fail_service(abi::v1::Status::internal_error);
        return abi::v1::Status::internal_error;
    }
    std::optional<Term> value;
    auto status = abi::v1::Status::ok;
    if (!calls.failure()) {
        const auto admitted = Term::from_word(result, owner_);
        if (admitted) {
            value = *admitted;
        } else {
            status = abi::v1::Status::invalid_argument;
            calls.fail_service(status);
        }
    }
    words_ -= frames_.back().count;
    frames_.pop_back();
    auto &handoff = frames_.empty() ? handoff_ : frames_.back().handoff;
    handoff = std::move(value);
    return status;
}

void GeneratedRoots::restore(std::size_t depth) noexcept {
    while (frames_.size() > depth) {
        words_ -= frames_.back().count;
        frames_.pop_back();
    }
    if (depth == 0) {
        handoff_.reset();
    }
}

std::size_t GeneratedRoots::depth() const noexcept { return frames_.size(); }

std::size_t GeneratedRoots::words() const noexcept { return words_; }

bool GeneratedRoots::contains(Word value) const noexcept {
    if (handoff_ && handoff_->word() == value) {
        return true;
    }
    for (const auto &frame : frames_) {
        if ((frame.handoff && frame.handoff->word() == value) ||
            std::ranges::contains(std::span(frame.slots.get(), frame.count), value)) {
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
