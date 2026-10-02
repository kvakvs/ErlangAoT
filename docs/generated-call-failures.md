# Generated-call failure contract (revision 2)

Current atom support (pattern/guard step 3): literal atoms and booleans, runtime-owned
module bindings and owned host/error atoms are implemented. The module descriptor
uses ABI revision 4; the revision-2 checked call channel is unchanged. See
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
| Successful expression/call | Checked target-word term; empty channel and owned result handoff | Consume the value |
| Pattern mismatch | Matcher continuation (steps 6/9) | Try next candidate; no diagnostic/channel mutation |
| Guard rejection | Guard continuation (steps 7/8) | Try next candidate; no diagnostic/channel mutation |
| Exhausted function clauses | `CallError::erlang_exception`, reason `function_clause` | Stop caller; host receives failure |
| Body match failure | Same error class, reason `badmatch`, owned offending `Term` | Stop caller; host receives failure |
| Ordinary record access failure | Same error class, reason `badrecord`, owned offending `Term` (step 17) | Stop caller; guard context instead rejects silently |
| Body service argument failure | Same error class, reason `badarg` (step 7) | Stop caller; guard context instead rejects silently |
| Body arithmetic failure | Same error class, reason `badarith` | Stop caller; guard context instead rejects silently |
| Body map access/update failure | Same error class, reason `badmap` or `badkey`, owned offending value/key | Stop caller; guard context instead rejects silently |
| Invalid lazy body left operand | Same error class, typed `badarg_value` and owned payload representing `{badarg, Value}` (step 8) | Stop caller; guard context rejects the enclosing alternative |
| Infrastructure failure | `CallError::runtime_failure` with exact `Status`, or existing native `CallError` | Stop caller; never treat as guard rejection |

Every admitted Erlang exception currently has class `error`. Reasons are typed IDs,
not runtime atom IDs. Payload admission covers arbitrary integers, finite floats,
owned atoms, canonical empty values, tuples, proper/improper lists, maps and
bitstrings. Exact-start heap indices and runtime ownership are checked before
access; malformed, foreign and unavailable identity words reject. Owned Terms
retain atom spelling or heap backing across invocation cleanup and retries;
expired context handles deny further heap access.
[Generated root scopes](generated-roots.md) transfer result/error ownership before
cleanup. Ordered clauses, body matches and numeric/map/record errors are executable.
Full `catch`/`try`, stack traces and source `throw`/`exit` remain with F20.

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
not implement handler consumption or asynchronous propagation. All admitted heap
payloads follow the rooted ownership contract; raw unvalidated heap words are rejected.

## Compatibility

Descriptor/runtime `abi::v1::version` is now **4**. The namespace and `eav1_` symbol
encoding retain the existing term/symbol representation. Revision-1/2/3 descriptors
and runtime options reject before metadata use. Startup references
`erlang_aot_register_module_v4`, so old runtime linking fails. Revision 3 appends
atom spellings/count to descriptors; generic signatures and the revision-2 checked
channel services are unchanged. All generated objects and native consumers must
be rebuilt. Revision 4 additionally requires [generated roots](generated-roots.md)
before allocating calls and return/error handoff before scope cleanup.

## Validation boundary

`codegen_failure_{O0,O2}-{on,off}` compiles two authored `.erl` modules through the
real semantic and lowering stages, then replaces three leaf bodies with explicitly
labeled native fault seams before optimization/emission. The separately configured
consumer links through `ErlangAoT::generated_program`, without LLVM. This is fault
transport evidence, not executable source pattern/guard support.

The retained graph includes a private local call, remote nesting, a subsequent
argument and a consuming outer call. Cases cover heap service/diagnostic failures,
function_clause, badmatch, rejected invalid/foreign payloads, builtin exceptions,
structured builtin errors, reentrant generated calls, and exceptions thrown from
native entries both with and without an already-recorded Erlang failure. Each failed invocation
skips later work, preserves the first failure, cleans its channel/accounting, and
is followed by successful retry on the same context. Error payloads survive retry.
Existing public CLI native/oracle tests supply unmodified success baselines, wrong
ABI/width rejection, failed-batch publication and all four optimization policies.
[Final validation](patternmatch-step20-validation.md) records the complete admitted
source corpus in both CLI drivers and keeps these injected seams separately labeled.
