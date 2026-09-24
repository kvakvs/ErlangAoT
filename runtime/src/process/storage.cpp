#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime {
ProcessHeap::ProcessHeap(ProcessContext &owner, HeapOptions options) : options_(options), owner_(owner) {}

ProcessHeap::~ProcessHeap() = default;

std::size_t ProcessHeap::used_words() const noexcept { return used_words_; }

std::size_t ProcessHeap::capacity_words() const noexcept { return capacity_words_; }

// Empty mailbox ownership only; step 13 supplies message/cursor storage and cancellation semantics.
class Mailbox::Impl final {
  public:
    // Retain a process-local binding without fabricating messages, cursors or scheduler state.
    explicit Impl(ProcessContext &owner) : owner(owner) {}

    // The owning context outlives this binding and its future receiver-owned roots.
    ProcessContext &owner;
};

Mailbox::Mailbox(ProcessContext &owner) : impl_(std::make_unique<Impl>(owner)) {}

Mailbox::~Mailbox() = default;
} // namespace erlang_aot::runtime
