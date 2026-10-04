#pragma once
#include "terms.hpp"
#include <erlang_aot/abi/roots.hpp>

namespace erlang_aot::runtime {
struct RootOptions {
    // Bound total live slots and nesting independently of retained heap backing.
    std::size_t words = 1'000'000;
    std::size_t frames = 4096;
    // Size the first stack segment; later segments double up to `words`, and a frame never spans two.
    std::size_t segment_words = 256;
};

class GeneratedRoots final {
  public:
    // Bind roots to one stable process owner; stack segments are allocated only at generated entry.
    explicit GeneratedRoots(ProcessContext &owner, RootOptions options = {}) noexcept;
    GeneratedRoots(const GeneratedRoots &) = delete;
    GeneratedRoots &operator=(const GeneratedRoots &) = delete;
    // Push a zeroed frame window transactionally, reporting exact infrastructure failures in the context.
    Word *enter(std::size_t count) noexcept;
    // Transfer a successful result before releasing the most recent frame; failures retain their payload roots.
    abi::v1::Status leave(Word *frame, Word result) noexcept;
    // Restore host-entry depth on all native exception/failure paths without touching older frames.
    void restore(std::size_t depth) noexcept;
    // Expose bounded root accounting for host lifecycle/fault invariants.
    std::size_t depth() const noexcept;
    std::size_t words() const noexcept;
    // Report reserved stack segment words, live or spare, for heap statistics.
    std::size_t capacity() const noexcept;
    // Inspect registered words and result handoffs without dereferencing candidate heap words.
    bool contains(Word value) const noexcept;

    // Visit every frame slot and result handoff word so a collector can rewrite it in place.
    template <typename Visitor> void visit(Visitor &&visit) {
        for (auto &frame : frames_) {
            for (auto &slot : std::span(frame.slots, frame.count)) {
                visit(slot);
            }
            visit_handoff(frame.handoff, visit);
        }
        visit_handoff(handoff_, visit);
    }

  private:
    struct Segment {
        // Stable backing for frame windows (BEAM Y registers); frames carve it bottom-up and release it LIFO.
        std::unique_ptr<Word[]> words;
        std::size_t capacity;
        std::size_t used;
    };

    struct Frame {
        // Window inside one segment, stable while live; count includes the reserved zero-arity slot.
        Word *slots;
        std::size_t count;
        std::size_t segment;
        // Root a nested result word (BEAM X register) until the parent publishes its own slot or exits.
        std::optional<Word> handoff;
    };

    // Visit one present handoff word.
    template <typename Visitor> static void visit_handoff(std::optional<Word> &handoff, Visitor &visit) {
        if (handoff) {
            visit(*handoff);
        }
    }

    // Choose the segment for the next window: the top one if it fits, else an empty successor or a new one.
    std::size_t segment_for(std::size_t count);
    // Release the top frame's window back to its segment.
    void pop() noexcept;
    // Free empty segments beyond one spare above the top frame (base size when empty), so deep recursion does
    // not pin capacity.
    void trim() noexcept;

    // Keep liveness/error ownership with the context; host access remains serialized.
    ProcessContext &owner_;
    RootOptions options_;
    // The process stack: segments in push order and the frames whose windows they hold.
    std::vector<Segment> segments_;
    std::vector<Frame> frames_;
    std::size_t words_ = 0;
    // Root the outermost return word until its host invocation has read it.
    std::optional<Word> handoff_;
};

class RootInvocation final {
  public:
    // Establish cleanup independently of the first-failure channel, including nested host invocations.
    explicit RootInvocation(GeneratedRoots &roots) noexcept;
    ~RootInvocation();
    RootInvocation(const RootInvocation &) = delete;
    RootInvocation &operator=(const RootInvocation &) = delete;

  private:
    // Restore only frames opened after this host boundary; preserve any enclosing generated caller.
    GeneratedRoots &roots_;
    std::size_t depth_;
};
} // namespace erlang_aot::runtime
