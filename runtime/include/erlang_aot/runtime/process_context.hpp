#pragma once
#include "mailbox.hpp"
#include "process_heap.hpp"
#include <erlang_aot/abi/v1.hpp>

namespace erlang_aot::runtime {
class Runtime;
class CodeServer;
class AtomStorage;

// Keep control/creation failures separate from Erlang exceptions and exit reasons.
enum class ProcessError : std::uint8_t {
    invalid_argument,
    unknown_process,
    stopped,
    unsupported_backend,
    resource_limit
};
template <typename Value> using ProcessResult = std::expected<Value, ProcessError>;

// Keep identity stable after exit and reject identities from other runtime instances.
class ProcessIdentity final {
  public:
    // Compare immutable registry keys, never addresses or process liveness.
    bool operator==(const ProcessIdentity &other) const noexcept = default;

  private:
    friend class SchedulerPool;
    friend class Runtime;

    struct RuntimeKey {
        // Keep runtime identity distinct from the per-runtime process serial at construction.
        std::uint64_t value;
    };

    // Allocate a non-reused runtime/serial pair; exhaustion fails creation.
    ProcessIdentity(RuntimeKey runtime, std::uint64_t serial);
    // Distinguish pools even when handles outlive pool destruction.
    std::uint64_t runtime_;
    // Monotonically allocated identity within one runtime, never recycled.
    std::uint64_t serial_;
};

// Surviving host bindings may inspect liveness without dereferencing a destroyed context.
// Access is owner-thread confined; this is not a GC root registry or a synchronization primitive.
class ContextLifetime final {
  public:
    ContextLifetime(const ContextLifetime &) = delete;
    ContextLifetime &operator=(const ContextLifetime &) = delete;
    ContextLifetime(ContextLifetime &&) = delete;
    ContextLifetime &operator=(ContextLifetime &&) = delete;
    ~ContextLifetime() = default;
    // Observe exit invalidation even if a host retains this token past runtime destruction.
    bool alive() const noexcept;

  private:
    friend class ProcessContext;
    // Only a real context may issue a lifetime token.
    ContextLifetime() = default;
    // Clear before releasing mailbox/heap state; future TermFactory bindings must retain/check this token.
    bool alive_ = true;
};

// Bind the existing TermFactory sketch to one process's storage and registered roots.
class ProcessContext final {
  public:
    // Keep context addresses stable for the full continuation/heap lifetime.
    ProcessContext(const ProcessContext &) = delete;
    ProcessContext &operator=(const ProcessContext &) = delete;
    // Invalidate host handles before releasing process-owned terms and heap storage.
    ~ProcessContext();
    // Borrow a token that expires/is invalidated before process-owned resources are released.
    std::weak_ptr<const ContextLifetime> lifetime() const noexcept;
    // Identify the running process without exposing mutable scheduler state.
    const ProcessIdentity &identity() const noexcept;
    // Allocate only on this context's owning scheduler thread.
    ProcessHeap &heap() noexcept;
    // Start selective receive through mailbox().begin_receive() on this process's owner thread.
    Mailbox &mailbox() noexcept;
    // Reserved declarations (steps 11–14): no code/atom service or send implementation is supplied yet.
    // Resolve loaded code through the runtime-wide server shared by scheduler workers.
    CodeServer &code_server() noexcept;
    // Share one runtime-owned atom identity/name table across every scheduler and process.
    AtomStorage &atom_storage() noexcept;
    // Post a message signal without awaiting handling; sending to a dead local pid is a no-op.
    ProcessResult<void> send(ProcessIdentity recipient, const Term &value);

  private:
    friend class Scheduler;
    friend class Runtime;
    friend class TermFactory;
    friend class Process;
    friend class ProcessHeap;
    friend class Mailbox;
    // Keep runtime binding and lifetime token alive through mailbox and heap teardown.
    class Impl;
    std::unique_ptr<Impl> impl_;
    // Own process term storage; destroy the mailbox before releasing heap-backed values.
    ProcessHeap heap_;
    // Retain future messages/receive state within this same owner; currently empty and lazy.
    Mailbox mailbox_;
    // Create only after runtime identity/ownership and heap limits are validated.
    ProcessContext(Runtime &runtime, ProcessIdentity identity, HeapOptions heap_options);
};

} // namespace erlang_aot::runtime
