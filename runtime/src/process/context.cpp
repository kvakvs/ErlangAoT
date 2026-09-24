#include <erlang_aot/runtime/runtime.hpp>

namespace erlang_aot::runtime {
class ProcessContext::Impl final {
  public:
    // Bind the runtime and identity once, issuing a separately retained token for future host bindings.
    Impl(Runtime &runtime, ProcessIdentity identity) : runtime(runtime), identity(identity) {}

    // Runtime-wide service bindings will be resolved here once their owning services exist.
    Runtime &runtime;
    // Keep identity valid independently of mutable scheduler state or storage addresses.
    ProcessIdentity identity;
    // Outlive context storage when a host pins the token; invalidate before mailbox/heap destruction.
    std::shared_ptr<ContextLifetime> lifetime{new ContextLifetime};
};

ProcessIdentity::ProcessIdentity(RuntimeKey runtime, std::uint64_t serial) : runtime_(runtime.value), serial_(serial) {}

bool ContextLifetime::alive() const noexcept { return alive_; }

ProcessContext::ProcessContext(Runtime &runtime, ProcessIdentity identity, HeapOptions options)
    : impl_(std::make_unique<Impl>(runtime, identity)), heap_(*this, options), mailbox_(*this) {}

ProcessContext::~ProcessContext() { impl_->lifetime->alive_ = false; }

const ProcessIdentity &ProcessContext::identity() const noexcept { return impl_->identity; }

ProcessHeap &ProcessContext::heap() noexcept { return heap_; }

Mailbox &ProcessContext::mailbox() noexcept { return mailbox_; }

std::weak_ptr<const ContextLifetime> ProcessContext::lifetime() const noexcept { return impl_->lifetime; }

} // namespace erlang_aot::runtime
