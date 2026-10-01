# Deferred-feature reporting

The feature catalog separates supported generic fallback from deferred semantics.
Capability checks, reached runtime service failures and explicit executable-output
requests use this reporting contract; ordinary invalid input retains ordinary errors.

The canonical [feature catalog](../abi/include/erlang_aot/abi/features.hpp) assigns
explicit, non-recycled IDs and stable diagnostic names. Each entry records its
owner, semantic/service boundary, deferred/implemented status, integration step and
focused reporting test. `abi_features` checks the ID/name compatibility snapshot;
`codegen_features` covers compiler reporting; `runtime_service_output` and
`runtime_feature_output` cover runtime service diagnostics and escaped context.
`runtime_features` retains sink refusal/exception and invalid-ID injection.
`codegen_placeholders` audits every deferred compiler-owned catalog entry through real
positional and project CLI calls at O0/O2, with and without verbosity. It verifies
exact markers, source/module/target context, nonzero exit, clean stdout and preserved
outputs. Implemented atom expressions retain their stable ID and are covered by
`codegen_atoms`. The catalog test references identify the actual owner workflows.
Step 11 additionally installs the
[builtin dispatch boundary](runtime-builtins.md). Step 14 installs
[runtime service placeholders](runtime-services.md) and distinguishes unknown BIFs
from explicitly known deferred signatures. Unavailable services report once; normal
registration and calls stay silent. Direct and subprocess tests exercise these owners.

## Owning boundaries

| Owner | Extension point | Integration |
|---|---|---|
| Compiler | Capability checks over the existing [AST](../compiler/include/erlang_aot/compiler/ast/), then binding/call-graph analysis | Steps 16–19 add `compiler/src/semantic/` and select catalog entries before lowering/publication |
| Compiler | Expression lowering under `compiler/src/codegen/` | Steps 17/24 add defensive rejection at reached lowering operations; normal verification/emission failures keep their ordinary diagnostics |
| Runtime terms/BIFs | [Term/TermFactory](../runtime/include/terms.hpp) and [Callable](../runtime/include/callable.hpp) | Steps 10/11/14 distinguish known unavailable services from invalid values or unknown BIFs |
| Runtime processes | [ProcessContext::send](../runtime/include/erlang_aot/runtime/process_context.hpp), [SchedulerService::run/execute](../runtime/include/erlang_aot/runtime/scheduler.hpp) | Step 14 reports execution/send attempts without invoking code or changing lifecycle state |
| Runtime memory | [ProcessHeap::allocate/collect](../runtime/include/process_heap.hpp), deferred atom collection | Step 14 reports heap service attempts; atom collection remains unimplemented despite the new non-collecting atom table |
| Runtime modules | [CodeServer](../runtime/include/code_server.hpp) unload boundary | Step 14 reports deferred unload; linked native registration stays supported; dynamic-image/descriptor loading awaits its ABI |
| Driver | Final executable output after the [frontend handoff](../compiler/src/driver/frontend.cpp) | Explicit `--output` reaches the deferred-linking owner after semantic analysis; default in-memory compilation stays supported |

There are no synthetic subsystem implementations behind these entries. The
catalog's step number identifies a relevant boundary/integration step, not a
promise to implement the deferred Erlang feature in that step. Before enabling a
feature, add its semantic/service tests and retire or refine its catalog entry and
capability path. Keep the numeric ID reserved rather than reusing it.

## Message and propagation contract

A reached owner emits exactly one message, independent of verbosity:

```text
[pattern matching] notimpl: src/example.erl:12:5 [module="example"] [target="native"] [operation="match argument"]
```

The shared [formatter](../abi/include/erlang_aot/abi/feature_diagnostic.hpp) omits
unknown context. Source-only diagnostics use `file:line:column`; zero coordinates
are unknown. Control bytes become `\xHH`; quotes and backslashes are escaped so
context cannot inject additional lines. UTF-8 bytes are retained. Formatting itself
is silent, and the returned message has no trailing newline.

The compiler's [reject_feature](../compiler/src/codegen/features.hpp) reports the
first feature failure of an incomplete batch. It copies available source/module
context into `CompilationDiagnostic`, latches failure, discards all staged output,
and writes the same formatted message to stderr (or an explicitly supplied stream).
It returns `false`; callers propagate that result. A diagnostic's `reported` field
marks the owner's delivery attempt, so a later driver must not print it again.
Already failed or completed batches cannot reenter reporting. Ordinary backend
errors keep `reported == false` for their normal consumer to render.

The runtime's [FeatureFailure](../runtime/include/erlang_aot/runtime/features.hpp)
belongs to one operation and borrows a host diagnostic sink. Construction and
status inspection are silent. Its first `report` formats and delivers a message;
subsequent calls return the latched status without retrying or replacing it.
Independent operations can report the same feature independently. The object is
noncopyable/nonmovable and must not be shared between concurrent operations.

A null sink callback selects stderr, with one complete newline-terminated write.
A custom callback receives a borrowed message without a newline and returns whether
it accepted delivery; it must copy retained text. The embedding runtime owns the
callback's state/lifetime. This host-side callback does not cross generated
service signatures. Exceptions from formatting or callbacks are contained and
returned as a reporting failure; no terms or successful results are fabricated.

[status.hpp](../abi/include/erlang_aot/abi/status.hpp) defines the scoped C++
`abi::v1::Status` enum with `std::uint8_t` underlying type for project service boundaries:

| Status | Value | Meaning |
|---|---:|---|
| `Status::ok` | 0 | No failure recorded by the reporter; construction is not a service implementation |
| `Status::not_implemented` | 1 | Known deferred feature; owner reported it |
| `Status::invalid_argument` | 2 | Unknown feature ID; ordinary invalid-input diagnostic, never `notimpl` |
| `Status::diagnostic_failure` | 3 | Formatting, callback or stderr delivery failed; operation still failed |

Compiler stream/formatting failures similarly latch
`CompilationResult::diagnostic_capture_failed()`. Neither reporter retries output
automatically after a failed delivery. Callers propagate errors without adding
another feature report; an eventual harness exits nonzero and tears down normally.

## Validation boundary

Focused tests cover every catalog entry, exact names/IDs, source and optional
context, escaped control bytes, invalid IDs, artifact invalidation, failure
propagation and both throwing/nonthrowing sink failures. Subprocess tests capture
real stdout/stderr and assert one report with a nonzero exit, plus silence for an
unused reporter. Runtime-only builds exercise reporting without LLVM. Capability
selection is covered by CLI workflows; native foreign-platform execution remains pending. Step 14 adds
[direct runtime service tests](runtime-services.md), including state preservation,
known/unknown BIFs and once-only reporting through nested service wrappers.

## Compiler capability boundaries

Default compilation reports located `[feature name] notimpl` diagnostics through
semantic capability analysis, including unused functions. Shared compiler IDs24/25
name send expressions and expression sequences; runtime message passing retains
its existing ID. Concrete defensive lowering entry points reject heap values,
dynamic calls, closures, exceptions, receive, send and sequences and clear staged
artifacts even if capability analysis was bypassed. Driver publication requires a successful CompilationResult before writing any
artifact. An explicit `--output` request fails with `[executable linking] notimpl`;
manifest output fields remain reserved metadata. No production linker is invoked.
The native O0/O2 consumer additionally reaches real allocation failure, verifies
its exact once-only stderr message, runs generated calls with unchanged heap
accounting, tears down explicitly and exits nonzero. Ordinary runs remain silent.

Atom collection is the sole runtime catalog reservation without a constructible
service owner: atom storage and registration bindings are still deferred. Its
reporter test validates the message contract only, and claims no collection path.
Other runtime entries are exercised by `runtime_services`/`runtime_service_output`:
TermFactory, BIF bridge, send, scheduler run/execute, allocation/GC and unload.
Allocation/sink/invalid-IR injections remain deliberate coverage exceptions.
