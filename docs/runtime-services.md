# Deferred runtime service boundaries

Compilation step 14 installs reporting placeholders at existing runtime boundaries.
Link `ErlangAoT::generated_program`; these services depend on neither the compiler
nor LLVM. Calls require host serialization, as do the existing runtime owners.

| Boundary | Catalog feature | Result on a valid deferred request |
| --- | --- | --- |
| `TermFactory` constructors | term services | `TermError::not_implemented` |
| `ProcessHeap::allocate` | allocation | `HeapError::not_implemented` |
| `ProcessHeap::collect` | garbage collection | `HeapError::not_implemented` |
| `ProcessContext::send` | message passing | `ProcessError::not_implemented` |
| `SchedulerService::run` | scheduling | `SchedulerError::not_implemented` |
| `SchedulerService::execute` | process execution | `SchedulerError::not_implemented` |
| `CodeServer::unload` | dynamic modules | `CodeError::not_implemented` |
| `abi::v1::dispatch_builtin` for a known deferred signature | builtins | `Status::not_implemented` |

Each reached placeholder owns one `[feature name] notimpl` report, with its operation
name, on stderr. Host APIs accept a borrowed `DiagnosticSink`; the term factory
retains its sink until destruction or reassignment. Formatting, callback and output
failures return `diagnostic_failure`, including allocation failure while formatting.
These operations never produce a term, allocation, collection statistics, send
acknowledgment, cooperative return or successful unload. Output words in the BIF
bridge change only on success.

[The shared adapter](../runtime/include/erlang_aot/runtime/features.hpp) translates
`FeatureFailure` status into the owning API's error enum. Callers propagate the
error without another report. A native wrapper translating an already-reported
service failure into `CallFailure` must set `reported = true`; the checked callable
and generated BIF bridge then preserve that report. Delivery failure propagates as
`CallError::diagnostic_failure`. An eventual host launcher must return nonzero for
unhandled failure and tear down normally; the subprocess tests exercise this pattern.

## Validation and state preservation

Heap allocation still validates nonzero word count, overflow and configured limits
before attempting deferred work. Unload rejects an unknown module with
`module_not_found`. Execution checks shutdown, runtime ownership, registration and
runnable/non-suspended state. Ordinary validation failures remain silent. Deferred
execution never calls `begin_dispatch`, changes state, invokes `ProcessCode`, starts
workers or grants reductions. Unload leaves registrations and pinned images intact.
Send reports unavailable even for self-send; it never creates a signal or touches
the mailbox. Signal ordering, admission and receive remain future work.

[TermFactory](../runtime/include/terms.hpp) now has a lightweight binding that retains
only a weak context-lifetime token and a borrowed sink. Construction, moves and
destruction do not allocate or register roots. Operations on expired bindings return
`expired_context` before reporting. All factory value constructors remain deferred,
even `integer`, `nil` and empty containers. Existing supported immediate construction
uses `encode_integer` and `Term::from_word`. Factory input semantics and atom/heap
ownership are not implemented by these placeholders. Operations taking opaque identity
or descriptor types remain inaccessible until those types have concrete definitions.

Raw term classifiers and `Term::from_word` remain silent validation utilities; rejecting
an unbound identity does not attempt a term service. Other semantic `Term` accessors
remain declarations. No atom table is fabricated: `AtomStorage` and its collection
entry remain catalog/API reservations until table ownership exists. File loading,
generated module descriptors and atom-root initialization have no defined service ABI
yet; native `CodeServer::load` continues to publish linked registries successfully.

## Known BIFs

[The bounded signature catalog](../runtime/include/erlang_aot/runtime/builtins.hpp)
reserves `erlang:self/0`, `length/1`, `spawn/3`, `spawn_link/3`, `send/2`, `make_ref/0`,
`garbage_collect/0`, `apply/3`, `tuple_size/1` and `+/2`. These names/arities follow the
local OTP reference's `erts/emulator/beam/bif.tab` and `erts/preloaded/src/erlang.erl`.
This is an explicit initial set, not a complete OTP BIF inventory or implementations.
Matching uses exact module/name/arity; there are no atom IDs or callable targets in
this catalog. It does not add compiler BIF lowering or expand executable syntax.

The bridge first resolves an actual native registration. A missing signature in
this catalog reports deferred `builtins`; any other missing signature returns the
new `Status::unknown_builtin` (11), silently and without modifying the output.
Wrong module or arity does not identify a known BIF. Host `CodeServer::resolve`
retains its distinct module/export lookup errors. Registered unavailable bodies
continue to report once through the checked invocation owner.

## Tests

`runtime_services` directly exercises the reachable boundaries, expired factory
bindings, sink failures, state preservation and teardown. `runtime_service_output`
checks one stderr line and nonzero failure exit per boundary, including propagation
from GC through a registered native wrapper and the BIF bridge. Supported lifecycle
and calls using injected sinks stay silent. Existing allocation-injection tests
verify failed reporting under host allocation exhaustion and balanced cleanup.
