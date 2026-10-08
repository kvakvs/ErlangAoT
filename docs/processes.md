# Processes

Plan 11 step 43 (2026-10-08): spawned processes on a cooperative executor;
step 44: exit reasons and error reports; step 45: sending messages; step 46:
selective receive; step 47: receive timeouts; step 48: links and exit
signals; step 49: monitors.

## Executor

One runtime runs all its processes on the thread that started the program
([execution model](execution-model.md)). Startup makes the entry function's
call the first process, the main process, and runs the executor until it
ends:

- Runnable processes wait in one first-in, first-out queue. The executor runs
  the first one for a time slice of 4,000 reductions (OTP's `CONTEXT_REDS`),
  then puts it back at the end of the queue, unless it ended.
- Every function entry spends one reduction: calls, tail calls, fun and
  dynamic calls, and builtins. With none left, the entry does not happen:
  the process records the function it was entering, keeps its arguments in
  its registers (they stay collection roots) and returns to the executor,
  which later resumes it by repeating the entry. A loop that makes no call
  (a comprehension over a list without calls in its body) runs to its end
  before the process can yield. Builtins whose work grows with a list or
  binary argument run in portions and yield between them
  ([portions](builtins.md#portions)).
- Each process owns its heap and stack. Arguments of a new process are copied
  into its heap ([copying between heaps](runtime-heap.md#copying-between-heaps)).
- A process ends when its first call returns or raises. An ended process's
  context, heap and stack are released at once; its pid stays a valid term
  and `is_process_alive/1` returns false for it.
- A process waiting in a `receive` is not in the queue; a send to it puts it
  back at the end ([receive](#receive)).
- When the main process ends, the program ends with its outcome
  ([executables](executables.md)): processes still queued or waiting are
  released without running further, as an OTP escript halts when `main/1`
  returns.
- An exit signal that ends the main process ends the program
  ([exit signals](#exit-signals)).
- `erlang:halt/0,1` in any process ends the program with its status. A
  runtime failure in any process (memory exhausted, an optional cap exceeded,
  an internal error) ends the program as a runtime failure (exit 70). An
  Erlang exception ends only the process that raised it ([exits](#exits)).
- Host invocations of exported functions (`erlang_aot_invoke_v1`) run their
  function in the calling context to completion, resuming it after each yield
  without running other processes; programs started by `erlang_aot_main_v1`
  use the executor.

## Exits

A process other than the main one ends with an exit reason, as in OTP
(`detail::exit_reason`, `process/exits`). Exit signals carry it to linked
processes ([links](#links)) and `'DOWN'` messages to monitoring ones
([monitors](#monitors)); error reports show it.

| How the process ends | Exit reason | Error report |
| --- | --- | --- |
| Its first call returns | `normal` | None |
| `exit(Reason)` (also `normal`, `kill`) | `Reason` | None |
| An error (`error/1,2,3`, `badarith`, `{badmatch, V}`, `undef`, ...) | `{Reason, Stack}` | Yes |
| An uncaught `throw(Value)` | `{{nocatch, Value}, Stack}` | Yes |
| An exit signal ends it ([exit signals](#exit-signals)) | The signal's reason, `killed` for `exit(Pid, kill)` | None |

An error report is written on stderr when the process ends, after flushing
standard output, in the format of OTP's default logger handler:

```text
=ERROR REPORT==== 8-Oct-2026::03:42:15.983000 ===
Error in process <0.8.0> with exit value:
{boom,[{crash_reports,'-main/1-fun-6-',0,[]}]}

```

The header has the local time; the reason is laid out as `~p` does. The main
process does not write one: its uncaught exception is the program's
([executables](executables.md)).

## Messages

`Dest ! Msg` and `erlang:send(Dest, Msg)` (plan step 45) evaluate `Dest`
first and return `Msg`.

| `Dest` | Effect |
| --- | --- |
| A pid of a live process | `Msg` is copied into the receiver's heap ([copying between heaps](runtime-heap.md#copying-between-heaps)), keeping its sharing, and appended to its signal inbox |
| A pid of a process that has ended | Nothing; the send succeeds |
| `{Name, Node}` of two atoms | Nothing (no name is registered until plan step 50, and there are no other nodes) |
| An atom, or anything else | `badarg` (an atom names no registered process yet) |

- Messages of one sender arrive in the order it sent them; a self-send is a
  message like any other.
- Every message is a collection root of its receiver until a receive takes it
  ([runtime heap](runtime-heap.md#roots-and-safe-points)).
- The receiver's mailbox (`Mailbox`) keeps a signal inbox, where sends append,
  and a message queue that receive scans from a saved position: arrived
  messages move behind the queue when a receive examines them.
- When the receiver's heap refuses the copy (an optional cap such as
  `--max-heap`, or the host out of memory), nothing is delivered and the
  sending process fails as a runtime failure (exit 70), like any process that
  exceeds a cap.

## Receive

`receive` (plan steps 46–47) selects among its clauses like `case`, over the
messages of the mailbox:

- The mailbox scan starts at the oldest message. Each message is matched
  against the clauses in order (patterns and guards, which may read bindings
  from before the `receive`); the first message some clause matches is
  removed and that clause's body runs. Messages no clause matches stay in the
  mailbox in their order; the next receive starts at the oldest message again.
- When every message has been examined, the process waits
  (`erlang_aot_wait_frame_v1`, a builtin entered like a call): it leaves the
  run queue until a send delivers a message to it, then the scan continues
  with the messages that arrived. Waiting processes keep their frames and
  messages as collection roots.
- Names bound in every clause, and in the `after` body when there is one, are
  exported after the `receive`, as for `case`; a call in a clause or `after`
  body's tail position is a tail call, so a server loop runs in constant
  stack.
- `after T -> Body`: `T` is evaluated first, before the scan. When the
  receive would wait, `T` must be `infinity` or an integer in
  0..4294967295 (milliseconds), else `error:timeout_value`; a message that
  matches at once never checks it, as in OTP. `after 0` runs `Body` as soon as
  every message has been examined. A finite timeout starts when the receive
  first waits and is not restarted by messages that match no clause; when it
  expires, `Body` runs from the bindings before the receive and the next
  receive scans from the oldest message. A message that arrives before the
  timeout expires is taken. A receive with only `after` is a sleep
  (`timer:sleep/1`'s idiom).
- Generated code: a loop head peeks at the next unexamined message
  (`erlang_aot_receive_v1` `peek`, into a root slot), clause selection takes a
  matched message (`take`) before its body or skips an unmatched one (`skip`)
  and loops; with no message left the loop enters the wait with the timeout,
  which answers `true` (scan again) or `false` (timed out: `restart`, then the
  `after` body).
- The executor keeps a timer per waiting process with a finite timeout: an
  expired one puts the process back in the queue, and when no process can run
  the executor sleeps until the earliest timer. Timeouts are measured on a
  monotonic clock in milliseconds and never fire early.
- When every process waits without a timeout for a message nothing can send,
  the program waits forever, as OTP's does. A host invocation (`erlang_aot_invoke_v1`)
  that would wait fails with `busy` instead: no other process runs during it.

## Links

Plan step 48. A link connects two processes both ways (`Signals` in each
context: the linked pids in link order and the `trap_exit` flag).

- `link(Pid)` links the caller to a live process and returns `true`; linking
  to itself or again does nothing. For a process that has ended it raises
  `error:noproc`, or, when the caller traps exits, returns `true` and sends
  the caller `{'EXIT', Pid, noproc}`, as OTP's local `link/1` does.
- `unlink(Pid)` removes the link on both sides; the link has no effect after
  it returns. `true` also when there was no link.
- `spawn_link/1,3` link the new process to the caller before it runs.
- When a process ends, every linked process gets an exit signal with its exit
  reason ([exits](#exits)) and the link is gone. Linked processes are signalled
  in the order the links were made.

## Monitors

Plan step 49. A monitor is one-way: the monitoring process holds it (by
reference, in `Signals`) and the monitored process keeps the reference and
the monitoring pid, so it can send the message when it ends.

- `monitor(process, Pid)` returns a new reference. When the process ends, the
  caller gets `{'DOWN', Ref, process, Pid, Reason}` with its exit reason
  ([exits](#exits)); for a process that has already ended it gets the message
  at once with reason `noproc`. Monitoring itself creates nothing. Every call
  makes a separate monitor with its own message; the messages of one process
  go out in the order the monitors were made.
- `demonitor(Ref)` stops the monitor: no `'DOWN'` of it arrives afterwards.
  It returns `true`, also for a reference that is not an active monitor of
  the caller.
- `demonitor(Ref, Options)`: `info` returns whether the monitor was still
  active; `flush` removes the oldest `{_, Ref, _, _, _}` message when it was
  not (its `'DOWN'` is already in the mailbox), as OTP does.
- `spawn_monitor/1,3` return `{Pid, Ref}`, the monitor made before the new
  process runs.
- A process that ends drops the monitors it holds.
- Monitors of registered names arrive with names (plan step 50).

## Exit signals

Every exit signal comes from the running process (`exit/2`,
`exit_signal/2`, `link/1`) or from the end of a process, so its target is
never running. The executor acts on it at once (`scheduler/signals`): an
ended target leaves the run queue or its wait and is finished (its links
signalled, its error report written, its context released) before the
sending builtin returns. A long linked chain ends process by process without
recursion.

| Signal at a process | Not trapping exits | Trapping exits |
| --- | --- | --- |
| `exit(Pid, kill)`, `exit_signal(Pid, kill)` | Ends with reason `killed` | Ends with reason `killed` |
| Reason `normal` (from a link, or sent to another process) | Nothing | `{'EXIT', From, normal}` message |
| `exit(self(), normal)` | Ends with reason `normal` (OTP's quirk) | `{'EXIT', Self, normal}` message |
| `exit_signal(self(), normal)` | Nothing | `{'EXIT', Self, normal}` message |
| Any other reason, including `kill` from a link | Ends with that reason | `{'EXIT', From, Reason}` message |

- `process_flag(trap_exit, Bool)` sets trapping and returns the previous
  setting (initially `false`).
- A process ended by a signal it sent itself (`exit(self(), kill)`) unwinds
  past every `catch`, `try` handler and `after` body: the signal is not an
  exception (`CallError::exited` in the failure channel).
- `exit/2` and `exit_signal/2` return `true`; to a process that has ended they
  do nothing. A reference destination does nothing either (there are no
  process aliases).
- An exit signal that ends the main process ends the program like an
  uncaught `exit` ([exit status](executables.md#exit-status)): reason `normal`
  exits 0.
- When the receiver's heap refuses an exit reason or `'EXIT'` message, the
  receiver fails as a runtime failure (exit 70).

## Builtins

| Builtin | Behavior |
| --- | --- |
| `spawn(Fun)` | `badarg` unless `Fun` is a fun; otherwise a new process calls `Fun()`, raising `{badarity, {Fun, []}}` in that process for another arity |
| `spawn(M, F, Args)` | `badarg` unless `M` and `F` are atoms and `Args` a proper list; otherwise a new process calls `M:F(Args...)`, raising `undef` in that process when no module exports it and no builtin has that name |
| `spawn_link(Fun)`, `spawn_link(M, F, Args)` | As `spawn`, and the new process is linked to the caller ([links](#links)) |
| `is_process_alive(Pid)` | `badarg` unless `Pid` is a pid; true while its process has not ended |
| `spawn_monitor(Fun)`, `spawn_monitor(M, F, Args)` | As `spawn`, returning `{Pid, Ref}` of a new [monitor](#monitors) |
| `link(Pid)`, `unlink(Pid)` | `badarg` unless `Pid` is a pid ([links](#links)) |
| `monitor(process, Pid)` | `badarg` for another type or item; a reference ([monitors](#monitors)) |
| `demonitor(Ref)`, `demonitor(Ref, Options)` | `badarg` unless `Ref` is a reference and `Options` a proper list of `flush` and `info` |
| `exit(Dest, Reason)`, `exit_signal(Dest, Reason)` | `badarg` unless `Dest` is a pid or reference; `true` after the [exit signal](#exit-signals) |
| `process_flag(trap_exit, Bool)` | The previous setting; `badarg` for another flag or a non-boolean |
| `erlang:send(Dest, Msg)`, `Dest ! Msg` | `Msg`, after sending it ([messages](#messages)); `send/2` is not auto-imported |

The new process is queued behind every runnable process; `spawn` returns its
pid at once.
