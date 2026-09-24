# Processes and scheduler — manual review skeleton

Status: context lifecycle implemented in compilation step 9, 2026-09-25; scheduling
remains proposed. [Runtime/context ownership](../../docs/runtime-lifecycle.md) now
provides startup, lazy heap/empty mailbox owners, lifetime invalidation and shutdown.
No workers, allocator, continuation or signal delivery are implemented. The remaining
C++23 declarations are review sketches, not a claim of OTP scheduling compatibility.

Read [scheduler.hpp](../include/scheduler.hpp) for creation/control and worker lifecycle,
[process.hpp](../include/process.hpp) for process state, signals and cooperative execution,
[process_context.hpp](../include/erlang_aot/runtime/process_context.hpp) for the implemented context,
[process_heap.hpp](../include/process_heap.hpp) for term storage/copying and the GC boundary,
and [mailbox.hpp](../include/mailbox.hpp) for selective-receive cursors and asynchronous reads.
Review headers live in `runtime/include/`; design notes live in this directory.

## Ownership and startup

`SchedulerPool::start()` creates one `Scheduler` with one OS worker thread per
available **logical CPU**, with a one-worker fallback if CPU discovery is unknown.
Physical versus logical cores is a review choice. Usable CPU discovery and optional
affinity need platform implementations for Windows, Linux and macOS; thread count
alone does not promise pinning. CPU hotplug and worker resizing are deferred.
Startup is transactional: join already-started workers if another worker fails.

The future pool will use the owning runtime's immutable runtime/serial identities
and route commands; each
scheduler exclusively owns its assigned processes, heap access, continuation
execution, run queue and cleanup. Default placement is round robin; callers may
choose a scheduler index. There is no migration or stealing in this first sketch.
An identity can outlive a process without retaining its heap. Counters never wrap
and identities are never reused; allocation fails at exhaustion.

`Scheduler::Impl` will hold the worker thread, mutex/condition variable, FIFO
command inbox, identity-indexed owning process registry, weighted ready entries,
current running pointer, optional realtime owner, live-process count and stop
state. Queue entries borrow stable registry-owned `Process` addresses. Registry
removal must first erase every queue/current/reservation reference. These private
containers are intentionally unspecified until implementation; `Process` already
shows the execution state and ownership fields they control.

## Cooperative execution and ticks

`ProcessCode::resume(context, budget)` models compiler-generated async code with
saved continuation state. The scheduler alone invokes it. Compiler-inserted safe
points debit `TickBudget::consume(work)`; subtraction saturates at zero, requiring
a return. Every dispatch gets the same positive finite tick grant. Ticks are work
units, not milliseconds, and no timer preempts a C++ function.

The return is `yielded`, `waiting` or `exited`. Yield means runnable; waiting parks
the continuation until a wake/message arrives; exit is terminal. Code completion
returns exited/normal. Host exceptions are caught at dispatch and become
code_failure. Continuation allocation failure during creation is reported before
publication. The eventual compiler ABI may use LLVM or C++ coroutines or an
explicit state machine; this virtual interface does not commit to their layouts.
The mailbox's C++ awaitable sketches the required suspension semantics; a
`ProcessCode` coroutine adapter must return control from `resume()` when the
awaitable parks, and resume its registered continuation only on a later scheduler
dispatch. Equivalent LLVM/state-machine lowering must preserve the same contract.

All blocking work must be asynchronous. An external completion posts a signal
instead of calling the continuation or accessing its context. Shutdown, suspension,
exit and signal delivery require code to return to a safe point. A continuation
that ignores its budget can stall its worker and pool destruction indefinitely;
there is no unsafe force-kill fallback. Nested async/tail-call frame layout and
root maps remain compiler/runtime integration work.

## Priority rules

| Priority | Selection contract |
| --- | --- |
| idle | Eligible only when it is the sole live process assigned to its scheduler. |
| low | Weight 1: one dispatch per eight normal dispatches under steady contention. |
| normal | Weight 8: baseline. |
| high | Weight 9: 1/8 more dispatches than normal, i.e. 9/8, not eight times normal. |
| realtime | Once selected, retains that scheduler until exit; no other process is dispatched. |

Weights apply **per process**, not per priority bucket. The intended implementation
is weighted virtual-service selection: choose the runnable process with smallest
service value, tie-break by FIFO enqueue order, and advance its service by
`72 / weight` after each dispatch (low 72, normal 9, high 8). Equal grants make
steady dispatch frequency approach 1:8:9. Early voluntary yields still consume a
dispatch; this is a frequency contract, not a CPU-time guarantee. A sole eligible
process always runs regardless of its weight.

On creation or return from waiting/suspension, clamp service to at least the current
virtual-time floor so dormant processes do not accumulate credit. Preserve service
when changing ordinary priority; use the new stride for future dispatches. Rebase
service values before overflow, preserving ordering. The floor advances with service
selection; when the weighted queue empties, retain its last floor. Tests must define
bounded deviation from ratios for finite runs before the queue is implemented.

Idle's live count includes waiting and suspended processes, and excludes reaped
ones. Consequently two idle processes on one scheduler cannot run, and an idle
process cannot run alongside a suspended normal process. This is the literal
"only one existing" rule requested. It can intentionally leave a worker asleep.

Pending runnable realtime processes take precedence over weighted/idle work; FIFO
breaks ties before ownership is acquired. A newly arrived realtime process takes
over only after the current ordinary dispatch returns. Once acquired, realtime
ownership survives **yield, waiting, explicit suspension and priority changes**
until that process exits. Waiting/suspension sleeps the worker with the reservation
intact; it does not dispatch another process. Commands continue to be serviced and
can wake, resume or exit the owner. Further realtime processes must wait too.
Each cooperative return grants the owner another finite budget when runnable;
unlimited scheduling tenure does not mean unlimited uninterrupted code execution.
Shutdown exits the owner before destroying the rest of the scheduler's processes.

The realtime waiting/suspension rule is a deliberate, literal reading of "never
switching until exit" and is a key manual-review decision. Allowing other processes
to run when it blocks would require a different contract.

## Commands, transitions and queue invariants

`SchedulerPool` exposes creation, suspend/resume, signals, exit, priority and
inspection APIs. Identity-based commands are safe to post from another worker or
host thread. Replies
are futures resolved after the owning worker applies or rejects a command; callers
must never block on a reply from runtime process code. Compiler async integration
will need a completion bridge to signals. FIFO is defined by insertion under the
inbox mutex; racing producers have no ordering before that serialization point.

Suspension is an idempotent boolean, separate from runnable/running/waiting/exited.
Resume clears it; it does not awaken a waiter. Handled message signals make a waiter runnable;
generic wake does so only for non-receive waits. Both leave suspension set.
Signals arriving during a dispatch are applied
after its return is recorded, before its next selection, so a wake cannot be lost
against a simultaneous transition to waiting. Wake received before a later dispatch
is only a notification, not a permanent permit; mailbox contents remain durable.
Receive-tail waits use the cursor's arrival-version handshake below; mailbox
nonemptiness alone is insufficient because every queued message may have been skipped.

| Event | Owning worker action |
| --- | --- |
| Create | Validate/adopt code, create context/heap, publish identity, enqueue once. |
| Select | Remove queue entry, set running, invoke one continuation. |
| Yield | Set runnable, drain pending controls, enqueue if eligible. |
| Wait | Set waiting, drain pending controls; enqueue only if awakened and unsuspended. |
| Suspend | Set suspension, remove any ready entry; acknowledge after running code returns. |
| Resume | Clear suspension, enqueue once if runnable. |
| Signal arrival | Append to the process signal inbox; mailbox and continuation remain untouched. |
| Handle wake/message | Consume the next signal; append a message to the mailbox, wake an eligible waiter, enqueue if unsuspended. |
| Exit/terminate | Mark terminal, erase ready/reservation references, destroy code then context/heap. |

Running, waiting, suspended and exited processes never appear in the ready queue.
An unsuspended runnable process has at most one entry; the retained realtime owner
is selected directly and has none. Idle entries may remain queued but ineligible
while other processes exist. Every live-count or admission change rechecks idle
eligibility and wakes the worker if necessary. Never spin on an ineligible queue.
Predicate checking and sleeping use the same inbox mutex to prevent lost wakeups.

The loop drains a bounded command batch, handles bounded process signal batches,
chooses work, grants ticks, records its return, applies pending controls and signals,
and then requeues/parks/reaps. Command and signal floods must
not prevent runnable work from receiving ticks. Natural completion wins over queued
controls after the dispatch: reap once and reject remaining commands as
unknown_process. For multiple exit commands, the first applied exit wins. Inspection
after reaping also returns unknown_process; persistent exit-history storage is not
part of this skeleton. Invalid options, a null continuation, zero tick grants and
invalid heap limits fail without publishing a process. The placeholder OS-thread
backend explicitly returns unsupported_backend.

The proposed scheduler `request_shutdown()` closes admission and wakes workers. Pending unprocessed
creation commands fail as stopped and release adopted continuations. Workers exit
existing processes at safe points and resolve all pending promises (including
controls for reaped processes) before joining. Destruction must occur on a host
thread, never on one of the pool's own workers. There are no detached workers.
Host allocation failures may throw `std::bad_alloc`, as in the existing term
sketch; accepted commands must still complete their promises during failure cleanup.

## Heap, term ownership and signals

Each implemented `ProcessContext` owns a lazy `ProcessHeap`, empty mailbox and
lifetime token. Root registration and `TermFactory` binding remain future work.
Explicit runtime shutdown requires contexts to be destroyed first; C++ RAII cleanup
invalidates remaining contexts before releasing reserved runtime-wide services.

Proposed allocation uses word-aligned chunks compatible with [term_layout.hpp](../src/terms/term_layout.hpp).
Appending chunks never relocates earlier allocations. Allocations are nonzero,
rounded to target words with checked arithmetic; limits cover total backing capacity,
including padding and unused chunk tails. Grow by at least one configured chunk,
or the request size if larger; clamp spare capacity to the remaining limit when
the rounded request itself still fits. Never reallocate/copy an existing chunk.

The heap adds terms with `add(value)`, equivalent to `value.copy_to(heap)`; both
return a rooted destination-owned graph. See [terms.md](terms.md#heap-ownership-copying-and-collection)
for deep-copy, identity, failure and thread-confinement rules. `TermFactory` also
constructs terms directly in the context's heap; heap storage owns cells, while
handles register roots rather than owning cells individually.

`collect()` explicitly permits future tracing/reclamation at a registered safe
point. The initial stub returns not_implemented, or unsafe_point when collection
is unsafe. `collection_placeholder()` before growth ignores not_implemented and
continues allocating. There is initially **no reclamation before exit**, no compaction
and no reuse of dead terms. When no chunk fits, return limit_exceeded or out_of_memory;
the code adapter chooses whether to exit with heap_limit. Future collection must
enumerate host roots, continuation roots, mailbox terms and cursor candidates,
then update references before resuming. Raw heap spans cannot survive a future moving collection without
an explicit pin/root protocol. Allocated term cells require no C++ destructors;
off-heap resources require separately registered cleanup.

`ProcessSignal::message(sender, term)` copies into an independently owned transit
buffer on the sender's scheduler thread. All signals, including messages and
self-sends, route through `Process::enqueue_signal()` into `signal_inbox_`.
Enqueueing neither touches the mailbox nor resumes process code. At convenient
owner-worker safe points, `Scheduler::handle_signals()` services process inboxes
using bounded `Process::handle_signals(budget)` batches. Each batch removes signals
in FIFO order and applies them through `Process::handle_signal()`; only handling a
message copies/decodes its payload into the receiver heap and calls the private
`Mailbox::append_handled_message()`. Wake and terminate signals never enter the mailbox.

Signal handling remains eligible while process code is waiting, explicitly suspended
or excluded by scheduling priority, so a waiting receiver can make progress. It does
not invoke the continuation or clear explicit suspension. If a batch exhausts its
budget, pending signals remain scheduled for service; the worker must not sleep with
unserviced inboxes. Termination discards remaining signals and completes their pending
diagnostic replies as unknown_process. Copying a `Term` handle alone is **not** message isolation. Root/lifetime
validation follows the term sketch. Message replies acknowledge successful mailbox
delivery, not merely acceptance into an inbox; allocation failure is reported and
must not partially append a message. Delivery to a dead process destroys its transit
buffer. Routing and handling preserve order across all signal kinds from one sender
to one recipient; concurrent senders have no ordering before inbox serialization.
Termination is an unconditional runtime
control, not a claim to implement Erlang trap_exit/link semantics.

This proposal supplies wake, message and terminate signals plus selective receive;
links, monitors, receive timeouts, trap_exit and arbitrary Erlang exit-reason terms
remain extensions. Signal
buffer/inbox/continuation memory needs separate resource budgets before production;
the proposed heap limit does not cover those allocations. Host term handles become
expired before heap release, as required by terms.md; they must not retain raw freed
storage. Step 9 implements context lifetime-token invalidation before mailbox/heap
release; wiring host term/factory roots to that token remains part of the term work.

## Sending and selective receive

`ProcessContext::send(recipient, value)` is the process-facing send entry. It validates
the sender-owned term, copies it into transit storage and posts a message signal without
waiting for the recipient. Success means accepted locally; sending to a dead local
pid succeeds as a no-op. Foreign-runtime identities and invalid term/owner inputs
report invalid_argument; source-copy or queue limits report resource_limit. A
generated Erlang send expression can return its original message term after this
acceptance. A later receiver allocation failure exits that recipient with heap_limit;
it cannot retroactively fail an accepted process-facing send.

`SchedulerPool::send(sender, recipient, value)` performs the same copy/routing but
returns a delivery reply, like `send_signal(..., ProcessSignal::message(...))`.
Call it only on the sender's owner thread; the synchronous copy must never read a
foreign worker's heap. Copy/validation failure produces an already-completed error
reply. Dead recipients report unknown_process on this diagnostic API. Host threads
may post already-owned signals without inspecting a process context. No send path
shares mutable term cells across processes. Sequential sends from one sender to one
recipient preserve order; messages from concurrent senders are ordered by delivery.

`context.mailbox().begin_receive()` creates one move-only cursor at the oldest
unconsumed message. A second active receive returns receive_active. Each
`co_await cursor.next()` advances to the next candidate and returns a rooted term
for compiler-generated pattern/guard checking. It **does not remove** that message.
Calling next again leaves the previous nonmatching candidate in its original
mailbox position. `cursor.receive()` removes only the currently selected candidate,
returns its rooted value and closes the receive session. The next receive starts
at the oldest remaining message, including messages skipped by the previous one.

At the mailbox tail, `next()` has no end/empty sentinel: awaiting it registers a
waiter and suspends the continuation, forcing `ProcessState::waiting` through the
scheduler. The owning Process records `receive_wait_`; the dispatcher honors it
even if an adapter reports yielded. This is a message wait, **not** the explicit
`suspended_` control flag. New delivery satisfying the pending read clears
`receive_wait_` and makes a waiting process runnable without
requiring an explicit resume; explicit suspension still blocks dispatch. A generic
wake or resume cannot complete a tail read without a candidate. Realtime tenure
remains in force while its receive waits, as specified above.

Mailbox edits and waiter registration occur only on the owner worker. Each message
has a stable ID and each arrival advances a checked version. On tail observation,
the cursor retains the last scanned ID/version, registers its waiter, then the
scheduler routes incoming signals, handles a bounded signal batch and rechecks the version before
parking. An arrival either supplies the pending read immediately or makes the parked
process runnable, never falls between checking and sleep. Version/ID exhaustion
must fail delivery instead of wrapping. A resumed cursor starts with newly arrived
messages after its saved position; it does not repeatedly rescan old unmatched
ones. A mailbox containing only skipped messages therefore parks rather than spins.

Only the scheduler may resume the saved coroutine handle, under a new tick grant.
Message arrival itself never invokes process code. Compiler loop safe points still
charge ticks while scanning an already-populated mailbox. Await operations are
confined to runtime-managed continuations: arbitrary host coroutines cannot borrow
the mailbox protocol. Constructing `next()` alone does not suspend or advance;
generated code must await it, and lowering must not treat it as an ordinary call.

There may be only one outstanding read per cursor. Concurrent reads or receiving
without a current candidate report a cursor error; completed/moved-from cursors
are invalid. Destroying the cursor cancels its session without removing messages;
process exit cancels waiter tokens before destroying continuation frames. Cancellation
must not destroy a coroutine while it is executing. The read retains only validated
session/lifetime state; generation checks prevent use-after-free or resuming a stale
handle. A successfully received message remains rooted by the returned Term even
after its mailbox entry is removed. Cursor IDs are not term pointers and therefore
survive moving GC; candidates and enqueued terms participate in heap root tracing.

Illustrative lowering (pattern matching and coroutine task types remain compiler work):

```cpp
auto opened = context.mailbox().begin_receive();
// Propagate a begin_receive error before taking its value.
auto cursor = std::move(opened.value());
for (;;) {
    auto candidate = co_await cursor.next(); // Automatically parks at the mailbox tail.
    // Propagate cursor errors; pattern/guard checks must also honor tick safe points.
    if (matches(candidate.value())) {
        auto selected = cursor.receive();   // Remove this message only; check the result.
        // Continue the selected receive clause using selected.value().
        break;
    }
}
```

Timeouts (`after`, including `after 0`) need a separate deadline/cancellation
contract and are deliberately not represented by this indefinitely waiting cursor.

## Review and implementation checkpoints

Review logical versus physical CPUs, literal idle eligibility, realtime reservation
while blocked, dispatch-frequency fairness, and the continuation boundary first.
Review heap limits, owned message transit and scheduler-mediated futures next.
The OS-thread process backend is only an enum/forward declaration, with no launcher.

After review, implement heap and process state, then the deterministic queue/worker
step function, then pool threading and command routing under `runtime/src/` with
approved headers under `runtime/include/erlang_aot/runtime/`. Keep deterministic
tests independent of real timing. Test 1:8:9 service with unequal class populations,
idle with other waiting/suspended processes, realtime ownership through every return,
duplicate enqueue prevention, wait/wake races, exit cleanup, stale identities,
shutdown replies, partial startup rollback, heap growth/address stability and limits.
Test cross-heap graph ownership and copy failures; selective matching behind skipped
messages; new receives restarting at the oldest unmatched entry; cursor progress
across tick yields/waits; arrival versus parking; explicit suspension plus delivery;
cursor cancellation, stale waiter tokens and mailbox/candidate roots across GC.
Add threaded stress plus supported-platform builds when workers are implemented.

Validation: combined C++23 syntax with warnings as errors (including the existing
term/layout headers), clang-format, repository clang-tidy rules and Lizard passed
on macOS arm64. Each new header also passes standalone syntax checking; a temporary
consumer validates co_await, receive, copying, collection and send API types. Lizard has
no new function bodies to measure. No full-build gate or commit was performed.
There is no executable behavior to test; these checks do not establish scheduling
fairness, thread safety or support for other platforms.
