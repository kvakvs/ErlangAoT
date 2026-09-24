#pragma once

// Worker/pool declarations remain sketches; the included SchedulerService is implemented without workers.
// See processes.md for ordering, queue invariants and literal realtime ownership.
#include "process.hpp"
#include <erlang_aot/runtime/scheduler.hpp>

#include <cstddef>
#include <future>
#include <memory>
#include <stop_token>
#include <vector>

namespace erlang_aot::runtime {
// Configure one new process without exposing queue or worker internals.
struct ProcessOptions final {
    // Use normal weighted service unless the caller explicitly chooses another policy.
    ProcessPriority priority = ProcessPriority::normal;
    // Reject the reserved OS-thread backend before consuming runtime resources.
    ProcessBackend backend = ProcessBackend::cooperative;
    // Bound process-private heap growth while GC is absent.
    HeapOptions heap;
    // Select a pool-local scheduler explicitly; default placement uses round robin.
    std::optional<std::size_t> scheduler;
};

// Report a copy of worker-owned state, never a Process pointer shared across threads.
struct ProcessSnapshot final {
    // Distinguish a running continuation from one eligible for a later dispatch.
    ProcessState state;
    // Report current scheduling class independently of suspension.
    ProcessPriority priority;
    // Expose the idempotent control gate alongside the preserved execution state.
    bool suspended;
    // Record fixed placement for diagnostics; work stealing is outside this sketch.
    std::size_t scheduler;
};

// Resolve only after the owner has applied/rejected a command; posting alone is not success.
template <typename Value> using ProcessReply = std::future<ProcessResult<Value>>;

// Own exactly one worker's process registry, control inbox and ready queue.
class Scheduler final {
  public:
    // Join the worker before destroying any process state; shutdown must already be requested.
    ~Scheduler();
    // Scheduler addresses and worker ownership remain stable for the pool lifetime.
    Scheduler(const Scheduler &) = delete;
    Scheduler &operator=(const Scheduler &) = delete;

  private:
    friend class SchedulerPool;
    // Bind a pool-local index and finite dispatch grant to one CPU worker.
    Scheduler(std::size_t index, std::uint64_t ticks_per_dispatch);
    // Hide native worker, mutex/condition variable, command inbox and process registry.
    class Impl;
    std::unique_ptr<Impl> impl_;

    // Start one scheduler OS thread; optional native affinity remains platform-specific work.
    void start();
    // Drain commands, select eligible work, grant ticks and finish each cooperative return.
    void run(std::stop_token stop);
    // Route a bounded command batch into process inboxes; retain signal replies until handling.
    // Apply other controls on this worker and complete their replies after state changes.
    void apply_commands();
    // Service bounded process signal batches at safe points, independently of code eligibility.
    void handle_signals();
    // Select realtime owner first, otherwise an eligible queued process using 1:8:9 weights.
    Process *select_next();
    // Mark running and invoke one continuation; catch host exceptions as code_failure.
    StepResult dispatch(Process &process);
    // Apply pending controls before requeueing, parking or exiting the returned process.
    void finish_dispatch(Process &process, StepResult result);
    // Enqueue once only when runnable, unsuspended and neither running nor the reserved realtime owner.
    void enqueue(Process &process);
    // Remove all queue/reservation ownership and destroy continuation/heap on this worker.
    void reap(Process &process, ExitReason reason);
    // Sleep on inbox/eligibility changes with a predicate checked under the inbox mutex.
    void wait_for_work(std::stop_token stop);
};

// Own one Scheduler per available logical CPU; a failed partial startup rolls back all workers.
class SchedulerPool final {
  public:
    // Discover the usable CPU set and start every worker; use one worker if discovery is unknown.
    static ProcessResult<std::unique_ptr<SchedulerPool>> start(std::uint64_t ticks_per_dispatch = 2000);
    // Request shutdown, join all workers and invalidate context handles before returning.
    ~SchedulerPool();
    // Keep runtime identity, process routing and thread ownership unique.
    SchedulerPool(const SchedulerPool &) = delete;
    SchedulerPool &operator=(const SchedulerPool &) = delete;

    // Adopt code and asynchronously publish a runnable process; validate options before enqueueing.
    ProcessReply<ProcessIdentity> create(std::unique_ptr<ProcessCode> code, ProcessOptions options = {});
    // Apply idempotent suspension at a safe point; no caller may mutate Process directly.
    ProcessReply<void> suspend(ProcessIdentity process);
    // Clear suspension; enqueue only if runnable, never spuriously wake an unsignalled waiter.
    ProcessReply<void> resume(ProcessIdentity process);
    // Enqueue in the process signal inbox; acknowledge handling, preserving explicit suspension.
    ProcessReply<void> send_signal(ProcessIdentity process, ProcessSignal signal);
    // Post a message signal from the sender's owner thread; acknowledge mailbox append after handling.
    ProcessReply<void> send(ProcessContext &sender, ProcessIdentity recipient, const Term &value);
    // Exit through the owner, including removal from ready queues and realtime reservation.
    ProcessReply<void> exit(ProcessIdentity process, ExitReason reason = ExitReason::requested);
    // Change weight at a safe point; an acquired realtime reservation lasts until process exit.
    ProcessReply<void> set_priority(ProcessIdentity process, ProcessPriority priority);
    // Obtain a consistent snapshot through the same serialized command inbox.
    ProcessReply<ProcessSnapshot> inspect(ProcessIdentity process);
    // Report the fixed worker count without reading mutable process state.
    std::size_t scheduler_count() const noexcept;
    // Close admission and wake workers; pending replies complete and every process exits cooperatively.
    void request_shutdown() noexcept;

  private:
    // Construct pool identity/routing before starting workers or allowing process creation.
    SchedulerPool();
    // Retain fixed worker ownership until cooperative shutdown joins every worker.
    std::vector<std::unique_ptr<Scheduler>> schedulers_;
    // Hide synchronized identity allocation, routing, admission and round-robin placement.
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace erlang_aot::runtime
