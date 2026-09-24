# Runtime lifecycle

Compilation step 9 implements startup, context ownership and shutdown in the
LLVM-free C++23 `erlang_runtime` library. The [C API](../abi/include/erlang_aot/abi/runtime.h)
uses ABI v1 opaque handles and fixed-width status codes. The
[host API](../runtime/include/erlang_aot/runtime/runtime.hpp) owns the same state.
These APIs do not yet execute Erlang processes or allocate terms.

## Linking and calling

Every generated-program consumer must link one matching target runtime through
`ErlangAoT::generated_program` (the alias of `erlang_generated_program`). With the
repository added as a CMake subdirectory and runtime builds enabled:

```cmake
add_executable(harness harness.cpp)
target_link_libraries(harness PRIVATE ErlangAoT::generated_program)
```

This interface supplies the runtime archive, ABI/runtime headers and its public
dependencies. Current lifecycle operations need no additional OS libraries. It
does not link the host LLVM SDK; runtime-only configuration never discovers LLVM.
Use Clang's C++ driver for the native harness. Foreign objects need a separately
built runtime for that target. Installation/export packaging and a production
launcher remain future work.

Initialize output handles to null, check every status, and destroy contexts before
shutting down their runtime. A complete minimal host example is
[link_consumer.cpp](../tests/runtime/link_consumer.cpp). It calls a native function
with the generated ABI shape; execution of compiler-emitted Erlang code is deferred.

`eaot_v1_runtime_start` accepts null options for defaults. Explicit options must
contain `EAOT_ABI_VERSION`, `sizeof(eaot_v1_term) * 8` and a positive maximum context
count (default 1024). `eaot_v1_context_create` accepts null options for a 64 KiB heap
chunk budget and 64 MiB heap limit. Explicit byte budgets must be nonzero target-word
multiples, with chunk size at most the limit. They reserve policy, not backing memory;
new heap usage and capacity are zero.

## Ownership and failure

The runtime uniquely owns stable context addresses. Each context owns a distinct
lazy heap and empty mailbox. Initialization publishes an output only after all
bookkeeping succeeds; a failure leaves the caller's output and existing contexts
unchanged. Heap/mailbox operations beyond lifecycle and heap accounting are still
declarations. No workers or pending signals are created by these APIs.

The runtime allocates non-recycled runtime/serial identities independently of raw
addresses. Identity exhaustion fails rather than wrapping. A raw pointer is a borrow,
not an identity: use only live issued handles, never manually delete a borrowed
context, and do not call through stale aliases after destruction. Successful C
destruction clears only the handle slot passed to it. Destroying another live
runtime's context fails without dereferencing or modifying that context.

Explicit runtime shutdown returns `BUSY` while any context remains and preserves
all runtime state. After destroying the contexts, successful shutdown releases the
runtime and nulls the C handle. Repeating shutdown on a null handle succeeds;
destroying a null context also succeeds when supplied a live runtime. Passing a
null handle-slot pointer is invalid. The C++ `Runtime` destructor additionally
provides RAII cleanup of any remaining contexts; a successfully stopped C++ owner
rejects further creation with `STOPPED`.

`ProcessContext::lifetime()` supplies a weak `ContextLifetime` token. A retained
token observes `alive() == false` before mailbox or heap destruction; it never keeps
the context alive. Context state survives both storage owners during teardown.
Future host-root/TermFactory metadata must retain or lock and check this token
before accessing a context. It is not a root registry, and the one-word `Term`
does not acquire checked host-handle behavior in this step.

Calls and token observations require host serialization per runtime. No call may
race creation, destruction, shutdown or resource access. Independent runtimes have
independent state; identity allocation alone uses a process-wide atomic counter.

All four C functions are nonthrowing and return a status; lifecycle success and
failure produce no stdout/stderr. Startup and creation contain C++ allocation and
unexpected exceptions. The existing feature-reporting statuses retain their values.

| Status | Lifecycle meaning |
|---|---|
| `OK` (0) | Operation completed |
| `INVALID_ARGUMENT` (2) | Null required pointer, occupied output, zero context cap or invalid heap budgets |
| `OUT_OF_MEMORY` (4) | Bookkeeping allocation failed; partial state was released |
| `BUSY` (5) | Explicit shutdown still has live contexts |
| `WRONG_OWNER` (6) | Context belongs to another runtime |
| `RESOURCE_LIMIT` (7) | Context cap, identity space or registry capacity exhausted |
| `STOPPED` (8) | C++ owner has already shut down |
| `ABI_MISMATCH` (9) | Explicit ABI version or term width differs |
| `INTERNAL_ERROR` (10) | Unexpected construction failure was contained |

## Reserved services and validation

Runtime state reserves empty ownership bindings for `CodeServer` and `AtomStorage`.
Their context accessors remain undefined until those services exist. Contexts are
destroyed first, then future code registrations, then the atom table so loaded-code
atom roots can be released in order. No fake service instance or successful service
result is supplied.

The [process sketch](../runtime/include/process.hpp) retains future signal-inbox and
continuation ownership. Once admission/execution exists, exit must discard pending
signals, resolve replies and release continuation/receive roots before heap teardown.
Those paths, term services, allocation, GC and scheduling remain subsequent steps.

Native macOS arm64 tests cover independent/repeated lifetimes, ownership errors,
limits, invalidation, shutdown ordering, silence and allocation-failure rollback.
The standalone consumer proves runtime-only linking and failure without the runtime.
ASan/UBSan cover lifecycle and injected failures; cross-target C header checks do
not establish native Linux/Windows runtime support.
