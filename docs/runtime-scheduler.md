# Scheduler lifecycle boundary

Step 13 implements one [SchedulerService](../runtime/include/erlang_aot/runtime/scheduler.hpp)
per runtime under `runtime/src/scheduler/`. `Runtime::scheduler()` borrows that
service while the runtime is active and returns null after successful shutdown.
All calls require host serialization with runtime/context operations. The service
tracks lifecycle records; it has no worker threads, run queues, CPU placement,
priority selection, continuations, signal delivery or receive implementation.
It neither selects nor executes Erlang code.

## Registration and ownership

Context creation remains independent of registration, so native-call harnesses
can use contexts without claiming process execution. `register_process(context)`
explicitly registers a live context owned by the same runtime as runnable and
unsuspended. The registry stores its immutable identity and lifecycle metadata,
never a borrowed heap or context pointer. Foreign contexts fail with `wrong_owner`.
Borrowed context references must be live when passed to registration.

Registration can succeed only once for each context. Duplicate registration, or
registration after removal, returns `already_registered`; a new process needs a
new context and identity. The once-only marker is set after successful registry
insertion. Backing allocation failures return `resource_limit` and preserve the
context's ability to retry. Registry size is bounded by the runtime's context cap;
retained vector capacity is bookkeeping, separate from each process heap budget.

`remove_process(identity)` retires a non-running registration and preserves the
context for host cleanup. `Runtime::destroy_context()` removes any remaining
non-running registration before invalidating its lifetime token and releasing its
mailbox/heap. It returns `Status::busy` for a running registration, preserving both
context and registry until a dispatch return is recorded. Unknown identities
return `unknown_process`; foreign-runtime identities return `wrong_owner`.
Removing an unregistered context through Runtime still works as before.

## State transitions

`ProcessState`, `StepDisposition` and `ExitReason` come from the existing process
sketch and now have shared declarations in
[process_state.hpp](../runtime/include/erlang_aot/runtime/process_state.hpp).
`inspect(identity)` returns a copied `ProcessLifecycle`, including the independent
suspension flag and an optional terminal reason. Exited records remain inspectable
until explicitly removed or their contexts are destroyed.

| Call | Preconditions | Result |
|---|---|---|
| `begin_dispatch(id)` | Admission open, runnable, unsuspended | Record running |
| `finish_dispatch(id, yielded)` | Running | Record runnable |
| `finish_dispatch(id, waiting)` | Running | Record waiting |
| `finish_dispatch(id, exited/reason)` | Running and valid reason | Record exited with terminal reason |
| `set_suspended(id, bool)` | Admission open, runnable or waiting | Change only the flag; repeated values succeed |
| `remove_process(id)` | Registered and not running | Retire the registration |

The dispatch methods record boundaries for a future executor or a host lifecycle
test. Future workers must apply these transitions on the owning worker and
reconcile the proposed `Process` fields with this single lifecycle authority. They do not call `ProcessCode::resume`, allocate a reduction grant, manipulate
a run queue or establish a GC safe point. Multiple registrations may independently
record in-flight state; worker placement and per-worker exclusivity are future work.
Malformed dispositions/terminal reasons return `invalid_argument` without changing
the running record. Other state violations return `invalid_transition`. A reason
is ignored for nonterminal returns, as specified by `StepResult`.

Controls on a running registration are rejected, rather than applied during a
dispatch. Future worker command handling must defer those controls until a safe
return boundary. Resume never changes waiting to runnable, and an exited process
cannot dispatch or return again. This step has no wake operation: waking waits for
the separate signal-handling boundary, not an implicit side effect of inspection,
registration or resume. All implemented service methods are nonthrowing and silent.

## Shutdown order

`request_shutdown()` idempotently closes registration, new dispatch and suspension
control admission. Inspection, in-flight returns and removal remain available so
a host can drain the registry. It does not destroy contexts or complete pending
work; no worker, future or continuation is created in this milestone. A busy
`Runtime::shutdown()` leaves admission unchanged. Once all contexts are destroyed,
explicit runtime shutdown releases its services and repeated shutdown succeeds.

Runtime RAII teardown closes admission and clears all lifecycle records first,
then invalidates/destroys contexts, then releases code registrations, then destroys
the stopped scheduler service. The future atom owner outlives those services.
This fallback may retire an unfinished bookkeeping dispatch because no code is
executing through this service. A future executor must first stop and join workers,
cancel receive waits and release continuation roots before using that teardown
order. Borrowed service pointers cannot survive runtime destruction; retaining a
loaded module does not retain its runtime or scheduler.

## Reserved execution and signal boundaries

The [worker/pool](../runtime/include/scheduler.hpp),
[process](../runtime/include/process.hpp) and [mailbox](../runtime/include/mailbox.hpp)
classes remain execution sketches. Their future implementation must preserve these
contracts from [the process design](../runtime/design/processes.md):

- `ProcessCode::resume(context, budget)` runs only on its owner worker with a finite
  positive `ReductionBudget`; compiler safe points consume work and return at
  exhaustion. Yield records runnable state before controls and signals are handled.
- Every message, including a self-send, owns transit data and enters the recipient's
  `Process::enqueue_signal` inbox. Enqueueing neither appends to the mailbox nor
  resumes code. FIFO preserves one sender's ordering across signal kinds.
- `Scheduler::handle_signals` uses bounded `Process::handle_signals` batches at
  owner safe points, including while waiting or explicitly suspended. Only handling
  imports a message into the receiver heap and calls `Mailbox::append_handled_message`.
  Wake/message handling preserves the explicit suspension flag.
- A generic wake cannot complete a pending receive read. Tail scanning registers a
  waiter and saves cursor position/version, then checks arrivals before parking.
  Delivery can supply a candidate or make an eligible waiter runnable; it never
  directly resumes a continuation or removes an unmatched message.

None of these signal/receive paths is enabled by the lifecycle registry. Priority
weights, realtime ownership, OS-thread backends, worker discovery and scheduling
fairness also remain proposals.

## Validation

`runtime_scheduler` covers once-only registration, growth/removal, invalid and
foreign identities, dispatch/return validation, suspension versus waiting, terminal
state, admission closure, automatic removal and registry/context/code teardown order.
`runtime_lifecycle_failure` injects allocation failures at startup and registration,
checking cleanup and retry without consuming an identity. Native macOS arm64 tests
and sanitizer runs validate the implemented bookkeeping only; native Linux/Windows
and actual Erlang scheduling remain pending.

Step 14 adds `SchedulerService::run` and `execute(identity)` reporting placeholders.
They return `not_implemented` (or `diagnostic_failure`) without running workers,
changing dispatch state or granting reductions. Execution checks registration and
runnable/non-suspended state; shutdown rejects both hooks silently. `ProcessContext::send`
now reports unavailable without enqueueing any signal. See [runtime services](runtime-services.md).
