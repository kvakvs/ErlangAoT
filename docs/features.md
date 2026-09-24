# Deferred-feature reporting

Compilation-plan step 8 defines the reporting contract. It does not install
capability checks, deferred runtime services or executable linking. Those handlers
arrive at their owning steps; ordinary CLI parsing and existing diagnostics retain
their behavior.

The canonical [feature catalog](../abi/include/erlang_aot/abi/features.hpp) assigns
explicit, non-recycled IDs and stable diagnostic names. Each entry records its
owner, semantic/service boundary, deferred status, planned integration step and
focused reporting test. `abi_features` checks the ID/name compatibility snapshot;
`codegen_features` and `runtime_features` exercise all their respective entries.
These are reporting-contract tests, not evidence that the future source/service
handlers have been installed.

## Owning boundaries

| Owner | Extension point | Integration |
|---|---|---|
| Compiler | Capability checks over the existing [AST](../compiler/include/erlang_aot/compiler/ast/), then binding/call-graph analysis | Steps 16–19 add `compiler/src/semantic/` and select catalog entries before lowering/publication |
| Compiler | Expression lowering under `compiler/src/codegen/` | Steps 17/24 add defensive rejection at reached lowering operations; normal verification/emission failures keep their ordinary diagnostics |
| Runtime terms/BIFs | [Term/TermFactory](../runtime/include/terms.hpp) and [Callable](../runtime/include/callable.hpp) | Steps 10/11/14 distinguish known unavailable services from invalid values or unknown BIFs |
| Runtime processes | [ProcessCode::resume and ProcessContext::send](../runtime/include/process.hpp), [scheduler workers](../runtime/include/scheduler.hpp) | Steps 13/14 add the service boundaries without making successful lifecycle calls invoke placeholders |
| Runtime memory | [ProcessHeap::allocate/collect](../runtime/include/process_heap.hpp), [AtomStorage::collect](../runtime/include/atom_storage.hpp) | Steps 12/14 report reached deferred operations; successful allocation or silent growth does not call the reporter |
| Runtime modules | [CodeServer](../runtime/include/code_server.hpp) module-loading boundary | Step 14 reserves dynamic-image hooks; later static descriptor registration must not be mislabeled as dynamic loading |
| Driver | Final executable output after the [frontend handoff](../compiler/src/driver/frontend.cpp) | Step 35 integrates the batch; executable linking remains reserved and must be diagnosed only when actually requested |

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
selection, real service placeholders and native foreign-platform
execution remain later work.
