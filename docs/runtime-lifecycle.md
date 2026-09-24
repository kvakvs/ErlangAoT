# Runtime lifecycle

The LLVM-free C++23 `erlang_runtime` library implements startup, context ownership
and shutdown through [Runtime](../runtime/include/erlang_aot/runtime/runtime.hpp).
All interfaces are private project C++ APIs; C compatibility and external consumers
are deferred until needed. These APIs do not yet execute Erlang processes or
allocate terms.

## Linking and calling

Every generated-program consumer must link one matching target runtime through
`ErlangAoT::generated_program` (the alias of `erlang_generated_program`). With the
repository added as a CMake subdirectory and runtime builds enabled:

```cmake
add_executable(harness harness.cpp)
target_link_libraries(harness PRIVATE ErlangAoT::generated_program)
```

This interface supplies the runtime archive, ABI/runtime headers, the C++23 language
requirement and public dependencies. Current lifecycle operations need no additional
OS libraries. It does not link the host LLVM SDK; runtime-only configuration never
discovers LLVM. Use Clang's C++ driver for the native harness. Foreign objects need
a separately built runtime for that target. Installation/export packaging and a
production launcher remain future work.

`Runtime::start()` returns `std::expected<std::unique_ptr<Runtime>, abi::v1::Status>`.
Check the result before use; RAII releases all owned state on scope exit.
`create_context()` returns an expected borrowed `ProcessContext*`, stable until the
runtime destroys that context. [link_consumer.cpp](../tests/runtime/link_consumer.cpp)
shows a complete checked lifecycle through the generated-program link target. It
calls a native function with the generated ABI shape; execution of compiler-emitted
Erlang code is deferred.

`RuntimeOptions` defaults to at most 1024 contexts, the current `abi::v1::version`
and `sizeof(abi::v1::TermWord) * 8` term bits. Startup rejects a zero context cap or
incompatible explicit ABI settings. `create_context()` defaults to a 64 KiB heap
chunk budget and 64 MiB heap limit. Explicit `HeapOptions` byte budgets must be
nonzero target-word multiples, with chunk size at most the limit. They reserve
policy, not backing memory; new heap usage and capacity are zero.

## Ownership and failure

The runtime uniquely owns stable context addresses. Each context owns a distinct
lazy heap and empty mailbox. Initialization publishes a successful result only
after all bookkeeping succeeds; failure preserves existing contexts. Step 12 adds
checked allocation/collection rejection and immediate-only copying; see
[process memory](runtime-memory.md). Receive operations remain declarations.
No workers or pending signals are created by these APIs.

The runtime allocates non-recycled runtime/serial identities independently of raw
addresses. Identity exhaustion fails rather than wrapping. A context pointer is a
borrow, not an identity: never manually delete it or use it after destruction.
`abi::v1::Context` aliases the forward-declared `ProcessContext`; no opaque C handle,
reinterpret cast or lifecycle adapter is involved. Destroying another live runtime's
context fails without dereferencing or modifying that context.

Explicit `shutdown()` returns `Status::busy` while any context remains and preserves
runtime state. After destroying the contexts, successful shutdown releases that
state. Repeating shutdown succeeds; further context creation/destruction returns
`Status::stopped`. Destroying a null context on a running runtime is invalid. The
`Runtime` destructor additionally provides RAII cleanup of any remaining contexts.

`ProcessContext::lifetime()` supplies a weak `ContextLifetime` token. A retained
token observes `alive() == false` before mailbox or heap destruction; it never keeps
the context alive. Context state survives both storage owners during teardown.
Future host-root/TermFactory metadata must retain or lock and check this token
before accessing a context. It is not a root registry, and the one-word `Term`
does not acquire checked host-handle behavior in this step.

Calls and token observations require host serialization per runtime. No call may
race creation, destruction, shutdown or resource access. Independent runtimes have
independent state; identity allocation alone uses a process-wide atomic counter.

Lifecycle methods are nonthrowing; success and failure produce no stdout/stderr.
Startup and creation contain C++ allocation and unexpected exceptions. The scoped
[Status enum](../abi/include/erlang_aot/abi/status.hpp) has an explicit `std::uint8_t`
underlying type and preserves the existing numeric values.

| Status | Lifecycle meaning |
|---|---|
| `ok` (0) | Operation completed |
| `invalid_argument` (2) | Zero context cap, invalid heap budgets or null context |
| `out_of_memory` (4) | Bookkeeping allocation failed; partial state was released |
| `busy` (5) | Explicit shutdown still has live contexts |
| `wrong_owner` (6) | Context belongs to another runtime |
| `resource_limit` (7) | Context cap, identity space or registry capacity exhausted |
| `stopped` (8) | Owner has already shut down |
| `abi_mismatch` (9) | Explicit ABI version or term width differs |
| `internal_error` (10) | Unexpected construction failure was contained |

## Reserved services and validation

Step 11 adds one runtime-owned [CodeServer](runtime-builtins.md), borrowed by every
context. `Runtime::code_server()` returns null after shutdown; live contexts expose
that same server by reference. AtomStorage and its accessor remain reserved.
Contexts are destroyed before code registrations, which precede the future atom
table. Resolved/module handles may retain code beyond runtime teardown, but do not
retain a process context. Future atom bindings must preserve their runtime lifetime
through those retained modules.

The [process sketch](../runtime/include/process.hpp) retains future signal-inbox and
continuation ownership. Once admission/execution exists, exit must discard pending
signals, resolve replies and release continuation/receive roots before heap teardown.
Those paths, backing allocation, GC and scheduling remain future work.

Native macOS arm64 tests cover independent/repeated lifetimes, ownership errors,
limits, invalidation, shutdown ordering, silence and allocation-failure rollback.
The standalone C++ consumer proves runtime-only linking and failure without the
runtime, plus exact context/status types. ASan/UBSan cover lifecycle and injected
failures. Native Linux/Windows runtime support remains unverified.
