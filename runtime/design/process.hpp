#pragma once

// REVIEW SKETCH ONLY: declarations without definitions, excluded from the build.
// Cooperative execution is a compiler continuation contract, not yet a C++ coroutine ABI.
#include "mailbox.hpp"
#include "process_heap.hpp"
#include "terms.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace erlang_aot::runtime {
class Scheduler;
class SchedulerPool;

// Preserve exactly the requested five scheduler classes; these are not OS priorities.
enum class ProcessPriority : std::uint8_t { idle, low, normal, high, realtime };
// Suspended is orthogonal to execution state, so a waiting process stays waiting on resume.
enum class ProcessState : std::uint8_t { runnable, running, waiting, exited };
// Reserve the alternate backend without implementing per-process native threads.
enum class ProcessBackend : std::uint8_t { cooperative, os_thread_placeholder };
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
    // Allocate a non-reused runtime/serial pair; exhaustion fails creation.
    ProcessIdentity(std::uint64_t runtime, std::uint64_t serial);
    // Distinguish pools even when handles outlive pool destruction.
    std::uint64_t runtime_;
    // Monotonically allocated identity within one runtime, never recycled.
    std::uint64_t serial_;
};

// Carry a runtime exit category; arbitrary Erlang reason terms remain a later extension.
enum class ExitReason : std::uint8_t { normal, requested, killed, code_failure, heap_limit, runtime_shutdown };
// A dispatch ends only at a compiler-inserted cooperative safe point or code completion.
enum class StepDisposition : std::uint8_t { yielded, waiting, exited };

struct StepResult final {
    // Tell the scheduler whether to enqueue, park or reap this continuation.
    StepDisposition disposition = StepDisposition::yielded;
    // Supply the terminal reason only when disposition is exited.
    ExitReason reason = ExitReason::normal;
};

// Count compiler-defined work units, never wall time; zero means a return is required.
// One unit of TickBudget should be roughly equal to one function call.
class TickBudget final {
  public:
    // Start a positive, finite grant; realtime receives fresh grants without switching owners.
    explicit TickBudget(std::uint64_t ticks);
    // Saturating subtraction reports whether execution may continue within this grant.
    bool consume(std::uint64_t ticks) noexcept;
    // Expose remaining work for bounded runtime helpers and compiler safe points.
    std::uint64_t remaining() const noexcept;

  private:
    // Prevent wraparound when one operation consumes more than the remaining allowance.
    std::uint64_t remaining_;
};

// Own a process-local asynchronous continuation between scheduler calls.
class ProcessCode {
  public:
    // Destroy suspended continuation state before releasing its process heap.
    virtual ~ProcessCode() = default;
    // Run bounded cooperative work; external callbacks must post signals, never resume directly.
    virtual StepResult resume(ProcessContext &context, TickBudget &budget) = 0;
};

// Transfer an owned signal; implementations must not retain sender-heap pointers.
class ProcessSignal final {
  public:
    // Wake a non-receive waiter; a pending mailbox read still requires a message.
    static ProcessSignal wake();
    // Request unconditional scheduler-mediated exit at the next safe point.
    static ProcessSignal terminate(ExitReason reason);
    // Copy a message into transit-owned storage on the sender's scheduler thread.
    static TermResult<ProcessSignal> message(ProcessContext &sender, const Term &value);
    // Transfer the signal's private buffer across scheduler command queues.
    ProcessSignal(ProcessSignal &&other) noexcept;
    ProcessSignal &operator=(ProcessSignal &&other) noexcept;
    // Release undelivered transit storage, including messages for dead recipients.
    ~ProcessSignal();
    // Disallow accidental sharing of mutable delivery state.
    ProcessSignal(const ProcessSignal &) = delete;
    ProcessSignal &operator=(const ProcessSignal &) = delete;

  private:
    friend class Scheduler;
    // Hide wake/exit variants and independently owned message graph encoding.
    class Impl;
    std::unique_ptr<Impl> impl_;
    // Construct only validated, owning signal envelopes.
    explicit ProcessSignal(std::unique_ptr<Impl> impl);
};

// Bind the existing TermFactory sketch to one process's storage and registered roots.
class ProcessContext final {
  public:
    // Keep context addresses stable for the full continuation/heap lifetime.
    ProcessContext(const ProcessContext &) = delete;
    ProcessContext &operator=(const ProcessContext &) = delete;
    // Invalidate host handles before releasing process-owned terms and heap storage.
    ~ProcessContext();
    // Identify the running process without exposing mutable scheduler state.
    const ProcessIdentity &identity() const noexcept;
    // Allocate only on this context's owning scheduler thread.
    ProcessHeap &heap() noexcept;
    // Start selective receive through mailbox().begin_receive() on this process's owner thread.
    Mailbox &mailbox() noexcept;
    // Copy and enqueue a message without awaiting delivery; sending to a dead local pid is a no-op.
    ProcessResult<void> send(ProcessIdentity recipient, const Term &value);

  private:
    friend class Scheduler;
    friend class TermFactory;
    friend class Process;
    friend class ProcessHeap;
    friend class Mailbox;
    // Own all process term storage; declared first so mailbox/host bindings are destroyed before it.
    ProcessHeap heap_;
    // Retain incoming messages and one active receive cursor independently of code dispatches.
    Mailbox mailbox_;
    // Keep scheduler binding, continuation roots and lifetime checks behind the existing term API.
    class Impl;
    std::unique_ptr<Impl> impl_;
    // Create only after scheduler identity/ownership and heap limits are validated.
    ProcessContext(ProcessIdentity identity, HeapOptions heap_options);
};

// Store scheduler-owned state; external callers operate through identity-based commands.
class Process final {
  public:
    // Destroy continuation first, context/roots second, and storage last.
    ~Process();
    // A process cannot move between owners by copying or moving the C++ object.
    Process(const Process &) = delete;
    Process &operator=(const Process &) = delete;

  private:
    friend class Scheduler;
    // Adopt an initially runnable continuation on its assigned scheduler.
    Process(ProcessIdentity identity, ProcessPriority priority, HeapOptions heap, std::unique_ptr<ProcessCode> code);
    // Keep process context alive through continuation destruction (reverse member order).
    ProcessContext context_;
    // Preserve compiler continuation state between cooperative dispatches.
    std::unique_ptr<ProcessCode> code_;
    // Choose eligibility and weighted dispatch frequency without OS priority changes.
    ProcessPriority priority_;
    // Distinguish runnable work, in-flight execution, awaiting signals and terminal state.
    ProcessState state_ = ProcessState::runnable;
    // Force a mailbox-tail return to park even if a code adapter incorrectly reports yielded.
    bool receive_wait_ = false;
    // Idempotent suspension blocks enqueueing without losing waiting/runnable state.
    bool suspended_ = false;
    // Enforce at most one run-queue entry, and no entry while running or exited.
    bool queued_ = false;
    // Retain one selected terminal cause through cleanup and completion reporting.
    std::optional<ExitReason> exit_reason_;
};

// TODO(threads): reserve a backend location; no OS-thread process class or launcher yet.
class OsThreadProcess;
} // namespace erlang_aot::runtime
