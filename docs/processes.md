# Processes

Plan 11 step 43 (2026-10-08): spawned processes on a cooperative executor.
Messages, receive, links and monitors arrive in later steps (45–49).

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
- When the main process ends, the program ends with its outcome
  ([executables](executables.md)): processes still queued are released
  without running further, as an OTP escript halts when `main/1` returns.
- `erlang:halt/0,1` in any process ends the program with its status. A
  runtime failure in any process (memory exhausted, an optional cap exceeded,
  an internal error) ends the program as a runtime failure (exit 70). An
  Erlang exception ends only the process that raised it; nothing is printed
  yet (crash reports: plan step 44).
- Host invocations of exported functions (`erlang_aot_invoke_v1`) run their
  function in the calling context to completion, resuming it after each yield
  without running other processes; programs started by `erlang_aot_main_v1`
  use the executor.

## Builtins

| Builtin | Behavior |
| --- | --- |
| `spawn(Fun)` | `badarg` unless `Fun` is a fun; otherwise a new process calls `Fun()`, raising `{badarity, {Fun, []}}` in that process for another arity |
| `spawn(M, F, Args)` | `badarg` unless `M` and `F` are atoms and `Args` a proper list; otherwise a new process calls `M:F(Args...)`, raising `undef` in that process when no module exports it and no builtin has that name |
| `is_process_alive(Pid)` | `badarg` unless `Pid` is a pid; true while its process has not ended |

The new process is queued behind every runnable process; `spawn` returns its
pid at once.
