#pragma once

// Execution, signals and continuations remain sketches; SchedulerService implements lifecycle bookkeeping.
// Cooperative execution is a compiler continuation contract, not yet a C++ coroutine ABI.
#include "mailbox.hpp"
#include "process_heap.hpp"
#include "terms.hpp"
#include <erlang_aot/runtime/process_context.hpp>
#include <erlang_aot/runtime/process_state.hpp>

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>

namespace erlang_aot::runtime {
class Scheduler;
class SchedulerPool;
class CodeServer;
class AtomStorage;

// Count compiler-defined work units (reductions), never wall time; zero means a return is required.
// One unit of ReductionBudget should be roughly equal to one Erlang function call.
class ReductionBudget final {
  public:
    // Start a positive, finite grant; realtime receives fresh grants without switching owners.
    explicit ReductionBudget(std::uint64_t ticks);
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
    virtual StepResult resume(ProcessContext &context, ReductionBudget &budget) = 0;
};

// Transfer an owned signal; implementations must not retain sender-heap pointers.
//
// Communication in Erlang is conceptually performed using asynchronous signaling.
// All different executing entities, such as processes and ports, communicate through
// asynchronous signals. The most commonly used signal is a message. Other common
// signals are exit, link, unlink, monitor, and demonitor signals.
//
// The only signal ordering guarantee given is the following: if an entity sends
// multiple signals to the same destination entity, the order is preserved; that
// is, if A sends a signal S1 to B, and later sends signal S2 to B, S1 is guaranteed
// not to arrive after S2.
class ProcessSignal final {
  public:
    // Wake a non-receive waiter; a pending mailbox read still requires a message.
    static ProcessSignal wake();
    // Request unconditional scheduler-mediated exit at the next safe point.
    static ProcessSignal terminate(ExitReason reason);
    // Copy into transit-owned storage for the recipient's signal inbox, never its mailbox directly.
    static TermResult<ProcessSignal> message(ProcessContext &sender, const Term &value);
    // Transfer the owned envelope through scheduler routing into a process signal inbox.
    ProcessSignal(ProcessSignal &&other) noexcept;
    ProcessSignal &operator=(ProcessSignal &&other) noexcept;
    // Release undelivered transit storage, including messages for dead recipients.
    ~ProcessSignal();
    // Disallow accidental sharing of mutable delivery state.
    ProcessSignal(const ProcessSignal &) = delete;
    ProcessSignal &operator=(const ProcessSignal &) = delete;

  private:
    friend class Process;
    // Hide wake/exit variants and independently owned message graph encoding.
    class Impl;
    std::unique_ptr<Impl> impl_;
    // Construct only validated, owning signal envelopes.
    explicit ProcessSignal(std::unique_ptr<Impl> impl);
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
    // Queue every signal on the owner worker without applying it or modifying the mailbox.
    ProcessResult<void> enqueue_signal(ProcessSignal signal);
    // Handle a bounded FIFO batch at a safe point, including while waiting or explicitly suspended.
    void handle_signals(ReductionBudget &budget);
    // Apply one dequeued signal; only a message copies into the heap and appends to the mailbox.
    TermResult<void> handle_signal(ProcessSignal signal);
    // Keep process context alive through continuation destruction (reverse member order).
    ProcessContext context_;
    // Own unhandled signals in arrival order; discard remaining transit storage on exit.
    std::deque<ProcessSignal> signal_inbox_;
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
