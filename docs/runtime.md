# Runtime

`erlang_runtime` is an LLVM-free C++23 library. It owns contexts, process heaps,
atoms, loaded modules and lifecycle bookkeeping. It does not yet run Erlang
processes: no scheduler workers, messaging or garbage collection. All APIs are
project-internal; host calls must be serialized per runtime.

## Linking

Link exactly one runtime built for the target through `ErlangAoT::generated_program`:

```cmake
add_executable(harness harness.cpp)
target_link_libraries(harness PRIVATE ErlangAoT::generated_program)
```

It brings the archive, ABI/runtime headers and C++23, but not LLVM.
[link_consumer.cpp](../tests/runtime/link_consumer.cpp) shows a full lifecycle.

## Lifecycle

[runtime.hpp](../runtime/include/erlang_aot/runtime/runtime.hpp):

- `Runtime::start(options)` → `std::expected<std::unique_ptr<Runtime>, Status>`.
  Defaults: 1,024 contexts, current ABI version and native term width,
  `max_atoms` 2^20.
- `create_context(heap_options)` → borrowed `ProcessContext*`, stable until
  destroyed. Heap defaults: 64 KiB chunks, 64 MiB limit (nonzero word multiples).
- `destroy_context(ctx)`, `shutdown()`: shutdown returns `busy` while contexts
  remain; after that it succeeds idempotently and later calls return `stopped`.
  The destructor cleans up remaining contexts.
- Context and runtime identities are non-recycled; exhaustion fails. A context
  pointer is a borrow, not an identity. `lifetime()` gives a weak token that
  reports `alive() == false` before heap teardown.
- Lifecycle calls never throw and are silent. Status values: [abi.md](abi.md#runtime-services).

Teardown order: close scheduler records → destroy contexts → release code
registrations → destroy scheduler service → atom table last. Resolved function
handles keep code and atom spellings alive after runtime teardown but never a
process.

## Process memory

Each context owns a stable `ProcessHeap` made of chunks that never move.

This heap is being replaced by the classic ERTS design in
[runtime-heap.md](runtime-heap.md); the rules below describe the current code.

- `allocate(words)` returns zeroed word storage. `reserve` gives a move-only
  reservation with explicit commit and automatic rollback. One reservation at a
  time per heap: build children first, reserve the parent last.
- Rejects zero, overflow, unsupported alignment and exhausted budget before
  publishing. Errors: `out_of_memory` (allocation) or `limit_exceeded` (budget);
  generated code receives the exact status.
- Resource-bearing cells register destructors at commit; teardown runs them in
  reverse order. Resources are never relocated as raw bytes.
- `used_words` includes padding; `capacity_words` counts retained backing.
- `collect()` reports `not_implemented`; nothing is reclaimed before teardown.

## Code server and builtins

Each runtime owns one `CodeServer` ([code_server.hpp](../runtime/include/erlang_aot/runtime/code_server.hpp),
[callable.hpp](../runtime/include/erlang_aot/runtime/callable.hpp)):

```cpp
auto functions = std::make_unique<ModuleRegistry>();
auto added = functions->add("identity", 1,
    [](ProcessContext &, std::span<const Term> args) -> CallResult<Term> { return args.front(); });
auto loaded = context.code_server().load({"native_demo", CodeImage::linked(), std::move(functions)});
auto fn = context.code_server().resolve({.module = "native_demo", .function = "identity", .arity = 1});
```

- A `ModuleRegistry` maps exact name/arity (≤ 255) to one all-`Term` callable.
  `load` freezes and publishes it; duplicates or failures publish nothing.
- `resolve` distinguishes missing module and missing export. `ResolvedFunction`
  pins the module image; `call` checks arity, arguments and results, and turns
  host exceptions into failures.
- Native bodies must be synchronous, non-blocking and must not retain the
  context or argument span.
- A bounded catalog of known deferred BIFs (`self/0`, `length/1`, `spawn/3`,
  `spawn_link/3`, `send/2`, `make_ref/0`, `garbage_collect/0`, `apply/3`,
  `tuple_size/1`, `+/2`) reports `not_implemented`; other unregistered names
  return `unknown_builtin` silently. No production BIFs are registered yet.
- `CodeServer::unload` is deferred; dynamic loading is not supported.

## Scheduler bookkeeping

`SchedulerService` ([scheduler.hpp](../runtime/include/erlang_aot/runtime/scheduler.hpp))
records process lifecycle only; it runs no code.

- `register_process(context)` once per context (same runtime); duplicates return
  `already_registered`. `remove_process(id)` retires a non-running record.
- `begin_dispatch` → running; `finish_dispatch` → runnable, waiting or exited
  (with reason). `set_suspended` toggles a flag on runnable/waiting records.
  Other transitions return `invalid_transition`.
- `request_shutdown()` closes registration, dispatch and suspension control;
  inspection and returns stay available for draining.
- `run` and `execute` report `not_implemented`.

Design sketches for workers, processes and mailboxes live in
[runtime/design/](../runtime/design/) and `runtime/include/{scheduler,process,mailbox}.hpp`.
Key intent: messages, including self-sends, enter the receiver's signal inbox
and are copied into its heap only when the owner handles signals; reductions
bound each resume.

## Standard output

`RuntimeOptions::standard_output` ([output.hpp](../runtime/include/erlang_aot/runtime/output.hpp))
receives `erlang:display/1` and later `standard_io` bytes. The default writes to
process `stdout` through C stdio (buffered); a host sink returns `false` to
report a failed write.

`erlang:display/1` renders its argument in [display style](terms.md#printing),
writes the text and a newline in one write and returns `true`. Rendering limits
and rejected writes become infrastructure statuses (`resource_limit`,
`output_failure`, ...) in the checked channel, never Erlang exceptions.

## Program startup

`erlang_aot_main_v1` ([startup.hpp](../abi/include/erlang_aot/abi/startup.hpp),
`runtime/src/startup/`) runs a whole program for the generated `main`: it
checks every descriptor's ABI, starts a default runtime, registers all modules
before any entry code, builds argv in the entry context, calls the entry and
maps the result to the exit status of [executables](executables.md#exit-status).
Reports go to stderr after stdout is flushed; the context and runtime are torn
down in order on every path. `erlang_aot_halt_v1` implements `erlang:halt/0,1`
(`abort` calls `std::abort`).

## Deferred services

These report one `[feature] notimpl` line ([features](features.md)) and change no
state:

| Boundary | Error |
| --- | --- |
| `TermFactory` pid/reference/fun/native-record constructors | `TermError::not_implemented` |
| `ProcessHeap::collect` | `HeapError::not_implemented` |
| `ProcessContext::send` | `ProcessError::not_implemented` |
| `SchedulerService::run` / `execute` | `SchedulerError::not_implemented` |
| `CodeServer::unload` | `CodeError::not_implemented` |
| `dispatch_builtin` on a catalogued BIF | `Status::not_implemented` |
