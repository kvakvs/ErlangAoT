#pragma once
#include "terms.hpp"
#include <erlang_aot/abi/roots.hpp>

namespace erlang_aot::runtime {
struct RootOptions {
    // Bound total live slots and nesting independently of retained heap backing.
    std::size_t words = 1'000'000;
    std::size_t frames = 4096;
};

class GeneratedRoots final {
  public:
    // Bind roots to one stable process owner; buffers are allocated only at generated entry.
    explicit GeneratedRoots(ProcessContext &owner, RootOptions options = {}) noexcept;
    GeneratedRoots(const GeneratedRoots &) = delete;
    GeneratedRoots &operator=(const GeneratedRoots &) = delete;
    // Register a zeroed buffer transactionally, reporting exact infrastructure failures in the context.
    Word *enter(std::size_t count) noexcept;
    // Transfer a successful result before releasing the most recent frame; failures retain their payload roots.
    abi::v1::Status leave(Word *frame, Word result) noexcept;
    // Restore host-entry depth on all native exception/failure paths without touching older frames.
    void restore(std::size_t depth) noexcept;
    // Expose bounded root accounting for host lifecycle/fault invariants.
    std::size_t depth() const noexcept;
    std::size_t words() const noexcept;
    // Inspect registered words and result handoffs without dereferencing candidate heap words.
    bool contains(Word value) const noexcept;

  private:
    struct Frame {
        // Stable buffers survive growth of the frame index; count includes the reserved zero-arity slot.
        std::unique_ptr<Word[]> slots;
        std::size_t count;
        // Pin a nested result until the parent publishes its own root slot or exits.
        std::optional<Term> handoff;
    };

    // Keep liveness/error ownership with the context; host access remains serialized.
    ProcessContext &owner_;
    RootOptions options_;
    std::vector<Frame> frames_;
    std::size_t words_ = 0;
    // Retain the outermost return until its host invocation has copied the owned Term.
    std::optional<Term> handoff_;
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
