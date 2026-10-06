#include <algorithm>
#include <bit>
#include <erlang_aot/runtime/process_context.hpp>
#include <new>

namespace erlang_aot::runtime {
namespace {
using abi::v1::frame_header_words;
using abi::v1::FrameDescriptor;

// Body of the runtime-owned bottom frame: returning into it ends the host invocation's native call.
void finish(void *) noexcept {}

// Descriptor of the bottom frame under each host invocation; it has no slots and no name.
constexpr FrameDescriptor bottom{nullptr, 0, 0, 0, &finish, 0, 0};
} // namespace

ProcessStack::ProcessStack(ProcessContext &owner, StackOptions options) noexcept : owner_(owner), options_(options) {}

bool ProcessStack::push(const FrameDescriptor &function) noexcept {
    live_registers_ = 0;
    const auto size = frame_header_words + function.slots;
    if (size > options_.limit_words - std::min(options_.limit_words, words_.size())) {
        owner_.generated_calls().fail_service(abi::v1::Status::resource_limit);
        return false;
    }
    const auto at = words_.size();
    try {
        words_.resize(at + size);
    } catch (const std::bad_alloc &) {
        owner_.generated_calls().fail_service(abi::v1::Status::out_of_memory);
        return false;
    }
    words_[at] = frame_;
    words_[at + 1] = std::bit_cast<Word>(&function);
    frame_ = at;
    return true;
}

void ProcessStack::pop() noexcept {
    const auto previous = words_[frame_];
    truncate(frame_);
    frame_ = previous;
}

void ProcessStack::truncate(std::size_t size) noexcept {
    live_registers_ = 0;
    words_.erase(words_.begin() + static_cast<std::ptrdiff_t>(size), words_.end());
}

const FrameDescriptor &ProcessStack::descriptor(std::size_t at) const noexcept {
    return *std::bit_cast<const FrameDescriptor *>(words_[at + 1]);
}

abi::v1::Code *ProcessStack::enter(const FrameDescriptor &function) noexcept {
    if (!push(function)) {
        registers_[0] = 0;
        return descriptor(frame_).body;
    }
    std::copy_n(registers_.begin(), function.arity,
                words_.begin() + static_cast<std::ptrdiff_t>(frame_ + frame_header_words));
    return function.body;
}

abi::v1::Code *ProcessStack::tail(const FrameDescriptor &function) noexcept {
    pop();
    return enter(function);
}

abi::v1::Code *ProcessStack::leave(Word result) noexcept {
    registers_[0] = result;
    pop();
    return descriptor(frame_).body;
}

Word ProcessStack::invoke(const FrameDescriptor &function, const Word *arguments) noexcept {
    auto &calls = owner_.generated_calls();
    if (!calls.active() || calls.failure()) {
        return 0;
    }
    const auto base = words_.size();
    const auto caller = frame_;
    if (push(bottom)) {
        std::copy_n(arguments, function.arity, registers_.begin());
        try {
            enter(function)(&owner_);
        } catch (const std::bad_alloc &) {
            calls.fail({CallError::resource_limit});
        } catch (...) {
            calls.fail({CallError::native_exception});
        }
    }
    const auto result = calls.failure() ? Word{0} : registers_[0];
    truncate(base);
    frame_ = caller;
    return result;
}

std::size_t ProcessStack::depth() const noexcept {
    std::size_t count = 0;
    for (auto at = frame_; at != none; at = words_[at]) {
        ++count;
    }
    return count;
}

bool ProcessStack::contains(Word value) const noexcept {
    for (auto at = frame_; at != none; at = words_[at]) {
        if (std::ranges::contains(std::span(words_).subspan(at + frame_header_words, descriptor(at).roots), value)) {
            return true;
        }
    }
    return false;
}

StackTrace ProcessStack::trace() const noexcept {
    StackTrace result;
    for (auto at = frame_; at != none && result.depth < StackTrace::limit; at = words_[at]) {
        if (descriptor(at).module) {
            result.frames.at(result.depth++) = &descriptor(at);
        }
    }
    return result;
}
} // namespace erlang_aot::runtime

namespace {
// Generated code receives continuation code as an untyped pointer.
void *code(erlang_aot::abi::v1::Code *body) noexcept { return reinterpret_cast<void *>(body); }

// The stack of a context passed to a generated-code service.
erlang_aot::runtime::ProcessStack &stack(void *context) noexcept {
    return static_cast<erlang_aot::runtime::ProcessContext *>(context)->stack();
}
} // namespace

void *erlang_aot_enter_v1(void *context, const void *frame) noexcept {
    return code(stack(context).enter(*static_cast<const erlang_aot::abi::v1::FrameDescriptor *>(frame)));
}

void *erlang_aot_tail_v1(void *context, const void *frame) noexcept {
    return code(stack(context).tail(*static_cast<const erlang_aot::abi::v1::FrameDescriptor *>(frame)));
}

void *erlang_aot_return_v1(void *context, erlang_aot::abi::v1::TermWord result) noexcept {
    return code(stack(context).leave(result));
}

erlang_aot::abi::v1::TermWord *erlang_aot_frame_v1(void *context) noexcept { return stack(context).frame(); }

erlang_aot::abi::v1::TermWord *erlang_aot_registers_v1(void *context) noexcept { return stack(context).registers(); }

erlang_aot::abi::v1::TermWord erlang_aot_invoke_v1(void *context, const void *frame,
                                                   const erlang_aot::abi::v1::TermWord *arguments) noexcept {
    return stack(context).invoke(*static_cast<const erlang_aot::abi::v1::FrameDescriptor *>(frame), arguments);
}
