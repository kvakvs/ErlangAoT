#pragma once
#include "callable.hpp"
#include "terms.hpp"
#include <algorithm>
#include <erlang_aot/abi/frames.hpp>
#include <erlang_aot/abi/modules.hpp>
#include <limits>

namespace erlang_aot::runtime {
struct StackOptions {
    // Optional per-process cap on the words of all frames, headers included; by default body recursion grows until
    // the host refuses memory.
    std::size_t limit_words = std::numeric_limits<std::size_t>::max();
};

// One flat, growable stack of explicit frames per process (docs/execution-model.md) plus its X registers.
// Frames link by offsets, so the block may move whenever a frame is pushed.
class ProcessStack final {
  public:
    // Bind the stack to its process; no words are allocated before the first frame.
    explicit ProcessStack(ProcessContext &owner, StackOptions options = {}) noexcept;
    ProcessStack(const ProcessStack &) = delete;
    ProcessStack &operator=(const ProcessStack &) = delete;

    // Push a frame for `function` and return its body, or record the budget failure and return the caller's body.
    abi::v1::Code *enter(const abi::v1::FrameDescriptor &function) noexcept;
    // Collect when the heap asks for it, keeping the first `live` registers as roots: the safepoint of a function
    // entry or loop head (docs/runtime-heap.md#collection-in-generated-code). Return whether it collected; a failed
    // collection changes nothing.
    bool safepoint(std::size_t live) noexcept;
    // Release the current frame for a tail call, then enter `function`.
    abi::v1::Code *tail(const abi::v1::FrameDescriptor &function) noexcept;
    // Pass `result` in the first register, release the current frame and return the body below it.
    abi::v1::Code *leave(Word result) noexcept;

    // Header of the current frame; valid until the next push.
    Word *frame() noexcept { return &words_[frame_]; }

    // Registers carrying arguments into and results out of every transfer.
    Word *registers() noexcept { return registers_.data(); }

    // Keep the first `count` registers as roots until the next push or pop, as a suspended entry's arguments.
    void keep_registers(std::size_t count) noexcept { live_registers_ = std::min(count, registers_.size()); }

    // Run `function` to completion above a bottom frame; frames left by native exceptions are released.
    Word invoke(const abi::v1::FrameDescriptor &function, const Word *arguments) noexcept;

    // Count live frames, bottom frames included, for leak checks.
    std::size_t depth() const noexcept;

    // Count the words of all live frames for leak checks.
    std::size_t words() const noexcept { return words_.size(); }

    // Report reserved stack words for heap statistics.
    std::size_t capacity() const noexcept { return words_.capacity(); }

    // Inspect the term slots of every frame without dereferencing candidate heap words.
    bool contains(Word value) const noexcept;
    // Name the innermost named frames, up to the stack trace limit, for a newly raised exception.
    StackTrace trace() const noexcept;

    // Visit every term slot of every frame, then the live registers, so a collector can rewrite them in place.
    template <typename Visitor> void visit(Visitor &&visit) {
        for (auto at = frame_; at != none; at = words_[at]) {
            for (auto &slot : std::span(words_).subspan(at + abi::v1::frame_header_words, descriptor(at).roots)) {
                visit(slot);
            }
        }
        for (auto &word : std::span(registers_).first(live_registers_)) {
            visit(word);
        }
    }

  private:
    // Offset marking the absence of a frame below the first one.
    static constexpr std::size_t none = static_cast<std::size_t>(-1);

    // Push a zeroed frame for `function` linked to the current one; false records the failure.
    bool push(const abi::v1::FrameDescriptor &function) noexcept;
    // Release the current frame.
    void pop() noexcept;
    // Drop every word from `size` on; shrinking never allocates.
    void truncate(std::size_t size) noexcept;
    // The descriptor named by the header at offset `at`.
    const abi::v1::FrameDescriptor &descriptor(std::size_t at) const noexcept;

    // Record failures in the owner's checked channel.
    ProcessContext &owner_;
    StackOptions options_;
    // Frame headers and slots; the size is the first free word.
    std::vector<Word> words_;
    // Offset of the current frame's header.
    std::size_t frame_ = none;
    // X registers; valid only across a transfer, never roots between invocations.
    std::array<Word, abi::v1::register_count> registers_{};
    // Leading registers that are roots: set by keep_registers, cleared by every push and pop.
    std::size_t live_registers_ = 0;
};
} // namespace erlang_aot::runtime
