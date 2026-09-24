#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime {
// Empty mailbox ownership only; message/cursor storage awaits receive and scheduler implementation.
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
