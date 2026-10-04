#pragma once
#include "terms.hpp"
#include <erlang_aot/abi/roots.hpp>

namespace erlang_aot::runtime {
struct RootOptions {
    // Bound total live slots and nesting independently of retained heap backing.
    std::size_t words = 1'000'000;
    std::size_t frames = 4096;
    // Fit each segment allocation, with its header and the allocator's, in one 4 KiB page; larger frames take
    // whole multiples.
    // A frame never spans segments, so its address is stable.
    std::size_t segment_bytes = 4096;
};

class GeneratedRoots final {
  public:
    // Bind roots to one stable process owner; stack segments are allocated only at generated entry.
    explicit GeneratedRoots(ProcessContext &owner, RootOptions options = {}) noexcept;
    GeneratedRoots(const GeneratedRoots &) = delete;
    GeneratedRoots &operator=(const GeneratedRoots &) = delete;
    // Free every segment, including ones left by frames still open at context teardown.
    ~GeneratedRoots();
    // Push a zeroed frame window transactionally, reporting exact infrastructure failures in the context.
    Word *enter(std::size_t count) noexcept;
    // Transfer a successful result before releasing the most recent frame; failures retain their payload roots.
    abi::v1::Status leave(Word *frame, Word result) noexcept;
    // Restore host-entry depth on all native exception/failure paths without touching older frames.
    void restore(std::size_t depth) noexcept;
    // Expose bounded root accounting for host lifecycle/fault invariants.
    std::size_t depth() const noexcept;
    std::size_t words() const noexcept;
    // Report reserved stack segment words for heap statistics.
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
        // Header of one allocation whose remaining words are frame slots (BEAM Y registers); segments chain
        // towards the stack base and each holds at least one frame while it exists.
        Segment *previous;
        std::size_t capacity;
        std::size_t used;
    };

    static_assert(sizeof(Segment) % alignof(Word) == 0, "slots must follow the header aligned");

    struct Frame {
        // Window inside the top segment at entry, stable while live; count includes the reserved zero-arity slot.
        Word *slots;
        std::size_t count;
        // Root a nested result word (BEAM X register) until the parent publishes its own slot or exits.
        std::optional<Word> handoff;
    };

    // Visit one present handoff word.
    template <typename Visitor> static void visit_handoff(std::optional<Word> &handoff, Visitor &visit) {
        if (handoff) {
            visit(*handoff);
        }
    }

    // View the slot words that follow a segment header in the same allocation.
    static std::span<Word> window(Segment &segment) noexcept;
    // Link a new top segment that fits `count` slots and, with allocator overhead, whole `segment_bytes` units.
    void push_segment(std::size_t count);
    // Release the top frame's window back to the top segment.
    void pop() noexcept;
    // Free the top segment once no frame uses it, so the top frame always lies in the top segment.
    void release_empty() noexcept;

    // Keep liveness/error ownership with the context; host access remains serialized.
    ProcessContext &owner_;
    RootOptions options_;
    // The process stack: the newest segment, linked to older ones, and the frames whose windows they hold.
    Segment *top_ = nullptr;
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
