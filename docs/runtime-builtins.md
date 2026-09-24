# Runtime builtin dispatch skeleton

Compilation step 11 implements native registration and checked synchronous calls
in [callable.hpp](../runtime/include/erlang_aot/runtime/callable.hpp) and
[code_server.hpp](../runtime/include/erlang_aot/runtime/code_server.hpp). Link the
`ErlangAoT::generated_program` target. The runtime has no compiler or LLVM dependency.
There are no production BIF implementations yet, and accepted Erlang syntax is unchanged.

Each active `Runtime` owns one `CodeServer`; every context borrows that server.
`Runtime::code_server()` returns null after shutdown. Publication and lookup require
host serialization, just like lifecycle operations. Native bodies are bounded,
synchronous functions: they must not block, retain the caller's context/argument
span, or destroy the active context/runtime during invocation. Worker execution,
concurrent publication, file loading and hot upgrades remain deferred.

A `ModuleRegistry` owns one `Callable` per exact function/arity/argument-type key.
Only all-Term signatures are implemented: each type sequence contains `arity`
copies of `typeid(Term)`, including an empty sequence at arity zero. Arity is bounded
at 255. Names are owned, exact strings with no normalization; nonempty names,
nonempty targets and unique keys are required. Registration consumes an rvalue
`Callable`; copy a reusable callable explicitly before passing it. Native typed extensions remain in
`runtime/include/unverified/`; any future implementation must use exact types and
explicit fallback without conversions. No second BIF table exists.

`CodeServer::load` takes a `ModuleDefinition` with its module spelling, nonnull
`CodeImage` and unique registry. Successful publication freezes that same registry,
including access through a previously retained draft alias. Invalid/duplicate
modules and allocation failures publish nothing and leave existing modules intact.
`resolve(FunctionRequest{.module = ..., .function = ..., .arity = ...})` selects the generic entry; a missing module and missing export have
distinct host errors. `ResolvedFunction` retains the module and its image. Direct
registry pointers borrow those owners and require a retained module handle.
Targets and captures are destroyed before their image, even if a resolved handle
outlives its runtime. Such a handle pins code only, never a process context.

Names are provisional host metadata. Atom-name overloads, atom roots, generated
module descriptors and ABI/word-width registration checks arrive in step 28.
Dynamic unload remains deferred. This skeleton neither creates an atom table nor
claims atom identities are valid based on their tag bits.

```cpp
using namespace erlang_aot::runtime;
auto functions = std::make_unique<ModuleRegistry>();
auto added = functions->add("identity", 1,
    [](ProcessContext &, std::span<const Term> arguments) -> CallResult<Term> {
        return arguments.front();
    });
// Check added before publication, and check loaded before lookup.
auto loaded = context.code_server().load(
    {"native_demo", CodeImage::linked(), std::move(functions)});
auto resolved = context.code_server().resolve({.module = "native_demo", .function = "identity", .arity = 1});
// Check resolved; call it with one validated immediate Term in the live context.
```

The minimal `Term` value supports only small integers and canonical empty tuple/list
words through `Term::from_word`. Copies are word copies with no heap roots.
`word()`, `kind()` and `integer_value()` are implemented; other semantic/heap
accessors and `TermFactory` remain reserved. The invalid default word is rejected
at invocation. Atom/pid/port construction reports `not_implemented`; heap tags and
malformed words fail without dereferencing. No ownership validation is fabricated:
only context-independent immediate values can cross this boundary today.

`ResolvedFunction::call` checks arity and every argument before entering the body,
validates successful results, preserves explicit failures and translates allocation
and other host exceptions. An unavailable body returns
`std::unexpected(CallFailure{CallError::not_implemented})`; the checked invocation
reports `[builtins] notimpl` once. The `reported` flag travels with failures through
nested checked calls. Failed diagnostic delivery becomes `diagnostic_failure`.
Direct `Callable` invocation bypasses these checks and is an internal caller's
responsibility. Bodies are never retried through another signature.

[abi/builtins.hpp](../abi/include/erlang_aot/abi/builtins.hpp) declares
`abi::v1::dispatch_builtin`: a native C++ service taking a live context, borrowed
module/function byte arrays and lengths, a term-word argument array, arity and an
output pointer. Its fixed-width `Status` return is separate from the Erlang word.
Only success writes output. Null arguments are allowed at arity zero; other
required null pointers, empty names, oversized arities and unsupported terms fail.
Non-null pointers must denote valid live objects/arrays for their declared lengths.
There are no STL values or exceptions across this generated-service boundary.
This does not alter the separate `GeneratedFunction` entry signature.

| Condition | Service status |
| --- | --- |
| Valid call/result | `ok` |
| Invalid pointers, names, arity, arguments or result | `invalid_argument` |
| Missing module/generic entry or unavailable body | `not_implemented`, one diagnostic |
| Registry/body resource exhaustion | `resource_limit` |
| Bridge allocation failure | `out_of_memory` |
| Other native exception | `internal_error` |
| Explicit ownership/liveness failure | `wrong_owner` / `stopped` |
| Diagnostic delivery failure | `diagnostic_failure` |

`runtime_builtins` covers keys, arities, freeze, shared target state and code/capture
lifetimes. `runtime_builtin_bridge` calls the service's actual native signature and
checks value/error propagation; `runtime_builtin_output` verifies silence and one
unavailable diagnostic. These are host registration/ABI tests; compiler BIF lowering
and execution of generated Erlang functions remain future work.
