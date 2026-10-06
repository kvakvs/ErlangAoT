#include <erlang_aot/runtime/runtime.hpp>
#include <utility>

namespace erlang_aot::runtime {
class ProcessContext::Impl final {
  public:
    // Bind the runtime and identity once, issuing a separately retained token for future host bindings.
    Impl(Runtime &runtime, ProcessIdentity identity, std::shared_ptr<detail::RuntimeMemory> memory)
        : runtime(runtime), identity(identity), memory(std::move(memory)) {}

    // Runtime-wide service bindings will be resolved here once their owning services exist.
    Runtime &runtime;
    // Keep identity valid independently of mutable scheduler state or storage addresses.
    ProcessIdentity identity;
    // Charge this process's heap and stack growth to the runtime-wide account.
    std::shared_ptr<detail::RuntimeMemory> memory;
    // Outlive context storage when a host pins the token; invalidate before mailbox/heap destruction.
    std::shared_ptr<ContextLifetime> lifetime{new ContextLifetime};
};

ProcessIdentity::ProcessIdentity(RuntimeKey runtime, std::uint64_t serial) : runtime_(runtime.value), serial_(serial) {}

bool ContextLifetime::alive() const noexcept { return alive_; }

ProcessContext::ProcessContext(Runtime &runtime, ProcessIdentity identity, HeapOptions options,
                               StackOptions stack_options, std::shared_ptr<detail::RuntimeMemory> memory)
    : impl_(std::make_unique<Impl>(runtime, identity, std::move(memory))), heap_(*this, options), mailbox_(*this),
      generated_calls_(stack_), stack_(*this, stack_options) {}

ProcessContext::~ProcessContext() { impl_->lifetime->alive_ = false; }

const ProcessIdentity &ProcessContext::identity() const noexcept { return impl_->identity; }

ProcessHeap &ProcessContext::heap() noexcept { return heap_; }

CodeServer &ProcessContext::code_server() noexcept { return *impl_->runtime.code_server(); }

AtomStorage &ProcessContext::atom_storage() noexcept { return *impl_->runtime.atom_storage(); }

OutputSink ProcessContext::standard_output() const noexcept { return impl_->runtime.standard_output(); }

Mailbox &ProcessContext::mailbox() noexcept { return mailbox_; }

std::weak_ptr<const ContextLifetime> ProcessContext::lifetime() const noexcept { return impl_->lifetime; }

const std::shared_ptr<detail::RuntimeMemory> &ProcessContext::memory() const noexcept { return impl_->memory; }

} // namespace erlang_aot::runtime
