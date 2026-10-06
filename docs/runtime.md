# Runtime

`erlang_runtime` is an LLVM-free C++23 library. It owns contexts, process heaps,
atoms, loaded modules and lifecycle bookkeeping. It does not yet run Erlang
processes: no scheduler workers or messaging, and the heap is collected only on
explicit host request. All APIs are
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
  Defaults: current ABI version and native term width, `max_atoms` 2^20
  (at most 2^26; programs set it with `--max-atoms`,
  [runtime options](executables.md#runtime-options)). The number of contexts
  is not limited.
- `create_context(heap_options, stack_options)` → borrowed `ProcessContext*`,
  stable until destroyed. Heap defaults: 233-word minimum heap
  (`min_heap_words`) and no memory cap (`limit_bytes` =
  `UNLIMITED_HEAP_BYTES`; a set budget is a word multiple at least the minimum
  heap). The stack is uncapped too unless `StackOptions::limit_words` is set.
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

Each context owns one `ProcessHeap`: a single heap block created by its first
allocation and sized `max(min_heap_words, request)`, plus a chain of heap
fragments owned by the same process. Words move only when the host calls
`collect()` at a safe point.

The heap follows the classic ERTS design; [runtime-heap.md](runtime-heap.md)
is its contract (layout, areas, sizing, admission, roots, collection).

- `allocate(words)` returns zeroed word storage. `reserve` gives a move-only
  reservation with explicit commit and automatic rollback. One reservation at a
  time per heap: build children first, reserve the parent last.
- Bump allocation fills the heap block; a request that does not fit goes to the
  newest fragment, else to a new fragment sized `max(min_heap_words, request)`
  (capped by the remaining budget). Words are word-aligned only. Rollback
  resets the area top, drops a fragment (or the heap block) created by the
  reservation and restores accounting exactly.
- Rejects zero, overflow and exhausted budget before
  publishing. Errors: `out_of_memory` (allocation) or `limit_exceeded` (budget);
  generated code receives the exact status.
- Binaries over 64 bytes live in shared buffers outside the heap. Each heap cell
  that refers to one holds a `std::shared_ptr` and joins the process's off-heap
  list when published; teardown walks the list and drops those references
  ([off-heap binaries](runtime-heap.md#off-heap-binaries)).
- `used_words` counts allocated words; `capacity_words` counts heap block and fragments;
  `off_heap_words` counts buffers this process created. Backing plus off-heap
  words share the optional `limit_bytes` budget; a collection keeps half of the
  budget left after survivors free, so exhausting it means the live data no
  longer fits ([failure behavior](runtime-heap.md#failure-behavior)).
- Every used word parses as a header-led object, a cons cell or filler
  ([word layout](runtime-heap.md#word-layout)); reserved words start zeroed.
  Raw `allocate()` words must stay zero or hold complete objects.
  `verify()` walks the heap block and every fragment and checks each term slot points at an object
  start of the same process (tests and debugging; `corrupt_heap` otherwise).
- `collect(roots)` copies everything reachable from the process roots and the
  host's root words into a new heap block, frees the old block and fragments,
  releases dead off-heap binaries and rewrites the roots
  ([collection](runtime-heap.md#collection)). Generated code collects at
  function entries and comprehension loop heads when `wants_collection()`
  ([collection in generated code](runtime-heap.md#collection-in-generated-code)).
  It runs only at a safe point (no
  generated code running outside a `SafePoint` scope, no open reservation),
  else `unsafe_point`; the [root inventory](runtime-heap.md#roots-and-safe-points)
  lists what it rewrites; failure
  to allocate the new block is `out_of_memory` with nothing changed.

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
| `ProcessContext::send` | `ProcessError::not_implemented` |
| `SchedulerService::run` / `execute` | `SchedulerError::not_implemented` |
| `CodeServer::unload` | `CodeError::not_implemented` |
| `dispatch_builtin` on a catalogued BIF | `Status::not_implemented` |
