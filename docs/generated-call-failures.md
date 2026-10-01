# Generated-call failure contract (revision 2)

Current atom support (pattern/guard step 3): literal atoms and booleans, runtime-owned
module bindings and owned host/error atoms are implemented. The module descriptor
uses ABI revision 3; the revision-2 checked call channel is unchanged. See
[runtime atoms](runtime-atoms.md) for ownership, limits and registration policy.

Step 2 of the pattern/guard plan implements the F20/F02 transport slice. Generated
functions still use target-width `TermWord(Context*, const TermWord*)`, but a return
word is usable **only when the context failure channel is empty**. Every non-tail
generated local/remote call checks `erlang_aot_call_failed_v2` before using its result
or evaluating another argument. Failure returns an invalid zero word immediately;
the word is never interpreted as a term. Specialization dispatch forwards directly
and returns without consuming the callee result, so its caller owns the check.

The channel is checked rather than encoding status into term bits or returning a
platform-dependent aggregate. LLVM never reads a C++ context/error layout. The
native C++ service takes an opaque context pointer and returns a byte; Microsoft
and Itanium linker spellings follow the selected target. Terms/arrays/descriptors
continue to derive widths/alignment from the target layout. Runtime stays LLVM-free.

## Outcomes and ownership

| Outcome | Transport | Consumer action |
| --- | --- | --- |
| Successful expression/call | Checked immediate return word; empty channel | Consume the value |
| Pattern mismatch | Matcher continuation (steps 6/9) | Try next candidate; no diagnostic/channel mutation |
| Guard rejection | Guard continuation (steps 7/8) | Try next candidate; no diagnostic/channel mutation |
| Exhausted function clauses | `CallError::erlang_exception`, reason `function_clause` | Stop caller; host receives failure |
| Body match failure | Same error class, reason `badmatch`, owned offending `Term` | Stop caller; host receives failure |
| Infrastructure failure | `CallError::runtime_failure` with exact `Status`, or existing native `CallError` | Stop caller; never treat as guard rejection |

Every admitted Erlang exception currently has class `error`. Reasons are typed IDs,
not runtime atom IDs. `erlang_aot_raise_v2` validates badmatch payloads using actual
host Term admission: small integers, canonical empty tuple/list and atoms owned by
the context's runtime. Invalid, heap, foreign atom and identity words reject without
dereferencing them. Atom Terms pin their spelling, so errors survive invocation
cleanup, independent retries and runtime teardown.
Heap payloads and roots are explicitly deferred to step 11 before heap admission.
Full `catch`/`try`, stack traces, `throw`/`exit`, source clause dispatch/body matching,
and `badarith` remain assigned to their later steps.

`GeneratedInvocation` owns a synchronous host scope on `ProcessContext`.
Registration's host adapter opens the scope, checks any pending failure before
entry, checks the channel before converting the returned word, copies the error,
and clears only on outermost scope exit. Nested registered callbacks share the
channel and cannot reset it. First failure wins. RAII also clears on host exception
unwinding; `ResolvedFunction::call` contains exceptions as native/resource failures.
Native services are noexcept. No exception propagates out of the checked host call.
A later independent invocation begins with no pending error.

The builtin bridge retains structured failures, publishes no output word on failure,
and uses `Status::erlang_error` separately from infrastructure statuses. A reached
heap allocation/collection failure records its exact service outcome in an active
channel; ordinary direct host service calls retain their existing expected/status
API and do not leave stale generated failure state. Deferred-service diagnostics
are emitted at their original owner once, with reporting state preserved downstream.
Other future generated runtime services must likewise record failures and require
a check before result use. Raw generated entry callers must establish a
`GeneratedInvocation`, inspect its channel before term conversion, and retain the
scope until copying the result/error; normal consumers should use resolved calls.
Error services reject calls outside an invocation. Execution remains owner-thread
serialized and synchronous; suspension/continuation state is outside this slice.

This channel gives later F20 handlers a place to inspect/move an owned exception
before resuming at an explicit handler. The current whole-invocation scope does
not implement handler consumption or asynchronous propagation. Step 11 must extend
payload/root ownership before adding heap values; no raw heap word may be admitted.

## Compatibility

Descriptor/runtime `abi::v1::version` is now **3**. The namespace and `eav1_` symbol
encoding retain the existing term/symbol representation. Revision-1/2 descriptors
and runtime options reject before metadata use. Startup references
`erlang_aot_register_module_v3`, so old runtime linking fails. Revision 3 appends
atom spellings/count to descriptors; generic signatures and the revision-2 checked
channel services are unchanged. All generated objects and native consumers must
be rebuilt.

## Validation boundary

`codegen_failure_{O0,O2}-{on,off}` compiles two authored `.erl` modules through the
real semantic and lowering stages, then replaces three leaf bodies with explicitly
labeled native fault seams before optimization/emission. The separately configured
consumer links through `ErlangAoT::generated_program`, without LLVM. This is fault
transport evidence, not executable source pattern/guard support.

The retained graph includes a private local call, remote nesting, a subsequent
argument and a consuming outer call. Cases cover heap service/diagnostic failures,
function_clause, immediate badmatch, rejected heap payloads, builtin exceptions,
structured builtin errors, reentrant generated calls, and exceptions thrown from
native entries both with and without an already-recorded Erlang failure. Each failed invocation
skips later work, preserves the first failure, cleans its channel/accounting, and
is followed by successful retry on the same context. Error payloads survive retry.
Existing public CLI native/oracle tests supply unmodified success baselines, wrong
ABI/width rejection, failed-batch publication and all four optimization policies.
