# Code server and native callable — manual review sketch

Status: proposed, 2026-09-20. Declarations and compile-time signature/codec constraints
only, outside CMake. There is no loader, registry, function adapter or conversion
implementation yet. This extends the [term](terms.md) and [process/scheduler](processes.md)
sketches without enabling more Erlang syntax or claiming executable runtime behavior.

- [code_server.hpp](code_server.hpp): modules, code lifetime, registration and resolution.
- [callable.hpp](callable.hpp): erased callable, per-invocation frame, results and limits.
- [native_callable.hpp](native_callable.hpp): `NativeCallable<Return(Arguments...)>`.
- [native_types.hpp](native_types.hpp): checked basic-type/container codecs and customization.

## Module storage and resolution

One runtime owns one `CodeServer`, shared by scheduler workers. A module definition
contains its atom `Term` name, a retained `CodeImage` and exported function registrations.
Each registration contains a function atom and a `shared_ptr<const Callable>`;
the callable supplies arity. Thus `math:sum/1` and `math:sum/2` are distinct keys,
and each name/arity may contain multiple registered argument signatures. Duplicate
name/arity/argument-type sequences are errors, even when return types differ. Arity
counts Erlang arguments, not a context parameter or list elements. Export listings
contain one entry per name/arity, not one per native variant.

`load(definition)` validates the complete definition before atomically publishing an
immutable `LoadedModule`. Reject null image/callables, invalid atom spellings or
ABI-unsupported arities, argument-type counts inconsistent with arity, duplicate
signatures, resource exhaustion and a module name
that is already loaded. A failed load leaves the registry unchanged and releases the
unpublished draft. `ModuleDefinition::name` and each `FunctionRegistration::name`
must be live atom Terms from the same runtime, accessed on their owner thread.
Registration extracts immutable process-independent spelling/atom metadata; it does
not retain their process roots in the loaded module. Non-atoms, expired bindings
and wrong-owner inputs report invalid_module or invalid_export as appropriate.
Keys are exact decoded UTF-8 atom spellings, with the same Unicode validity/length
rules as the term atom API. Do not normalize, case-fold or intern
arbitrary failed lookup strings. Empty spellings follow the atom API's validity rules.

`resolve(module, function, arity)` looks up the current module and its exported
function family, returning a `ResolvedFunction` retaining its signature index and
optional all-Term fallback. Argument selection occurs on prepare, not on MFA lookup.
Module absence is module_not_found; a
missing function/arity is function_not_exported. Names are separate parameters,
so quoted Erlang atom spellings containing punctuation need no ambiguous textual
`module:function/arity` parser. The overload taking atom Terms validates and extracts
the same keys on their owner thread; the string overload permits host metadata lookup
without accessing process heaps. Private/local function calls are resolved within the
generated module and do not become exports merely because they have code addresses.

`find_module` and `loaded_modules` return immutable owning snapshots, not references
into a mutable map. `LoadedModule::exports(context)` materializes function atom Terms
in the requesting process, returning checked allocation/ownership failures.
`LoadedModule::name()` offers an immutable spelling view for host-side inspection.
Listings sort by exact UTF-8 bytes and then arity. `Impl` contains
the synchronized module-name map, admission/resource limits and immutable per-module
function/arity indexes. Readers take a shared lock long enough to retain a snapshot;
load/unload publish/remove under an exclusive lock. No native function, custom
destructor or loader callback runs under registry locks. Build module indexes before
publication, and release removed module references after unlocking.

`unload(name)` only removes the current name mapping. Existing module snapshots,
resolved functions and prepared call frames retain their code. A later load of the
same name affects new resolutions; an old resolved handle continues to call its old
module. This is lifetime-safe removal, not Erlang's two-version hot-upgrade or purge
protocol. Automatic reload, hard purge, `on_load`, BEAM bytecode loading and SO/DLL
file discovery remain later work. Duplicate load is deliberately not silent replacement.

## What loaded code owns

Compiled atom literals are read-only constants initialized by runtime AtomStorage
calls before module publication. Each loaded module instance retains its initialized
atom bindings and metadata roots, including while resolved handles/call frames pin
unloaded code. A shared CodeImage does not contain runtime-independent numeric atom
IDs; distinct runtimes have separate bindings. See the
[compiled atom constant contract](atom_storage.md#compiled-atom-constants) for ordering,
failure cleanup and initialization-context lifetime.

`CodeImage` is the lifetime anchor for executable memory, module constants, relocation
data and platform loader handles. `CodeImage::linked()` represents statically linked
code that remains mapped for the executable's lifetime. A future native object/shared
library loader owns a specialized image whose destructor frees its resources.
CodeServer accepts already-prepared modules; it does not pretend to load `.erl` or
`.beam` files. Generated code needs a `Callable` adapter around its registered entry
descriptor, while C++ registrations use `NativeCallable`.

`LoadedModule` owns its export adapters and code image. `ResolvedFunction` retains
the module and its immutable overload set. Each preparation selects one adapter
and wraps the resulting frame with
the same module pin, so releasing the resolved handle or unloading the module cannot
unmap code used by that frame. Destroy frames/adapters/captured native objects before
releasing image storage: their destructors may themselves reside in that image.
Direct calls to an adapter's `prepare()` do not supply a module image pin; loader-backed
calls must go through `ResolvedFunction`. Statically linked direct adapters may be
used by a harness that owns their code lifetime explicitly.

Registrations and images must not capture process-local `Term` handles or borrowed
process contexts. Runtime-wide constants need immutable runtime storage, and are
materialized/rooted in the calling process when needed. Stop and join process workers
before destroying the server/runtime services. Surviving snapshots can retain image
storage, but invocation still requires a live caller context. Concurrent destruction
of the server while another thread calls its API is outside the lifecycle contract.

## Native function template

`NativeCallable<R(A0, A1, ...)>` represents one fixed Erlang arity. The argument list
contains unqualified **value** types accepted by `NativeArgument`; the result must
satisfy `NativeReturn` or be `void`. Unsupported signatures leave the template
incomplete and fail at compile time. There is no default-argument expansion, numeric
promotion, variadic argument pack, automatic reference borrowing or optional-argument
coercion. Register each desired arity and native argument signature explicitly.

The two constructors accept `std::function<R(Args...)>` or
`std::function<R(ProcessContext&, Args...)>`. The latter injects the current context
without counting it as an Erlang argument, allowing construction through `TermFactory`
and access to process services. Function pointers and explicitly selected overloads
can be wrapped directly. Copyable lambdas/functors may capture runtime-owned data.
The declared value signature also accepts a target taking compatible `const T&`
parameters through `std::function`; those references borrow exactly typed owned arguments only
for the duration of the body and must not escape. An empty std::function is an invalid
binding, rejected at construction with `std::invalid_argument`.

All module callers share the registered target. A target's captured mutable state
must be synchronized and callable concurrently from different schedulers; the adapter
does not acquire a global execution lock. Capture destructors must not require a
particular process worker. Move-only targets and alternative storage wrappers are a
possible later extension; the initial sketch uses `std::function` ownership.

## Dispatch without argument conversion

Dispatch checks registrations against the caller-supplied argument types; it never
calls `NativeCodec::decode` to make arguments fit a candidate. There are two entry paths:

- `prepare(context, span<const Term>)` selects only an all-Term registration. A boxed
  integer remains a Term for selection; its value/kind does not turn it into `int64_t`.
- `prepare_native(context, NativeArguments{values...}, optional_term_arguments)`
  selects an exact registered native signature. If there is no match, it requires
  an all-Term registration and uses only the explicitly supplied Term arguments.

`NativeArguments` owns the caller's already-typed values. Type matching compares the
ordered `std::type_index` keys published by `Callable::argument_types()`. Type aliases
for the same C++ type match; distinct widths, signedness, float/double, string forms
and container types do not. No matching inspects values, checks numeric ranges,
traverses elements, allocates conversion buffers or invokes custom codecs. Native
C++ type identity is local to the compatible host/runtime C++ ABI, not a persistent
wire ID; a future plugin ABI needs registered stable type descriptors instead of
assuming arbitrary independently built RTTI identities are interoperable.

| Supplied arguments | Registered variants | Selected behavior |
| --- | --- | --- |
| Native `int64_t` | `int64_t`, all-Term | Exact `int64_t` variant. |
| Native `int64_t` | `double`, all-Term | All-Term fallback, using explicit Term arguments; no widening. |
| Native `vector<int64_t>` | `list<int64_t>`, all-Term | All-Term fallback; no container conversion. |
| Boxed integer Term | `int64_t`, all-Term | All-Term variant, without unboxing. |
| Boxed list Term | `vector<int64_t>` only | no_compatible_callable; never decode the list automatically. |

The generic signature is precisely `Term, Term, ...` at the same arity. Mixed
signatures such as `(int64_t, Term)` are exact native specializations, not partial
wildcards or a generic fallback. Return types and injected ProcessContext do not
participate in selection. At arity zero the empty signature is also generic; only
one such registration is permitted for a name/arity. Distinct exact signatures are
unambiguous, so registration order and conversion cost never rank overloads.

If no exact variant and no generic registration exist, report no_compatible_callable
before running a body. If generic exists but a native caller has not supplied its
Term arguments, report generic_arguments_required. The optional span distinguishes
an explicitly supplied empty fallback from no fallback. Wrong argument counts report
bad_arity. Direct use of a typed adapter's boxed prepare, or a mismatched native pack,
reports argument_type_mismatch; a direct adapter cannot discover fallback registrations.

The caller/compiler is responsible for explicitly preparing equivalent Term values
when it wants fallback; dispatch neither boxes native inputs nor decodes boxed ones.
The generic function receives these original roots and implements its own checking,
pattern matching or explicit conversions. Do not touch fallback arguments when an
exact signature matches. On fallback validate their arity, liveness and ownership,
then retain roots before discarding the unused native pack. A selected target's
execution or result-encoding error never triggers another variant or retries its body.

## Explicit codecs and result encoding

The codecs remain utilities available to native code or explicit caller preparation.
The following mappings are not automatic call-time argument conversion rules.
Result encoding into the caller's heap is unchanged.


| C++ type | Explicit Term decoding / result encoding |
| --- | --- |
| `Term` | A live immutable root owned by the caller; retain it without implicit cross-heap copying. |
| Signed/unsigned integral types up to 64 bits | Exact integer, with checked signedness and range; bool and encoding-specific character types are separate. |
| `float`, `double` | Finite Erlang float; integer-to-float coercion is not implicit. |
| `bool` | Exactly the atoms `true` and `false`. |
| `std::string` | UTF-8 binary with explicit byte length, including embedded NUL; never an implicit atom. |
| `std::u32string` | Proper list of Unicode scalar integers (an Erlang character list). |
| `std::vector<T>`, `std::deque<T>`, `std::list<T>` | One proper list, decoded/encoded element by element in order. |
| `std::array<T, N>` | One proper list of exactly N elements on input; all N elements on output. |
| Other admitted owning iterable of T | Proper list on output; input additionally requires ordered `push_back` construction or an explicit codec. |
| `void` return only | The atom `ok`. |

Integer narrowing never wraps or truncates. Full-range `uint64_t` must use exact
integer construction (including the term library's decimal/bignum boundary when
needed), not a cast through `int64_t`. Floats are finite; conversion from binary64 to
C++ float must round-trip exactly or return out_of_range, including underflow/overflow.
Returning float widens exactly to binary64. Long double, C strings/raw pointers,
`string_view`, spans, initializer_list, borrowed ranges, range views and reference
signature types have no default codec. Use owned values or explicitly pass a Term.

Strings are owning copies, never references into Erlang binary/list storage. Validate
UTF-8 and Unicode scalar bounds when explicitly decoding or encoding; dispatch
itself does not validate native strings to select a target.
`vector<unsigned char>` is a list of integers, not a binary; choose `Term` for raw
binary bytes. Containers of strings and nested containers work recursively if their
element codec supports the required direction. An empty sequence maps to nil.
Improper lists fail, and fixed-size arrays fail on either too few or too many elements.

An arbitrary iterable can describe output order without providing a safe way to
reconstruct itself from a list. The generic decoder therefore requires a default
constructor and order/multiplicity-preserving `push_back`. Sets can be output lists
in iteration order, but have no generic input decoder because insertion can remove
duplicates; maps/pairs require explicit semantics and codecs. Unordered-container
iteration is not made deterministic by the adapter. Custom containers must actually
own their element storage: C++ concepts cannot prove lifetime or insertion semantics.
The encoder requires const iteration, so stateful non-const-only generators need an
explicit codec/materialization step.

`NativeCodec<T>` is the extension point. A checked static `decode(context, term,
budget)` supplies an explicit Term-to-value utility; `encode(context, value, budget)`
supplies return-value encoding. Neither codec participates in compatibility selection. They may be implemented independently. A custom input container that uses a
different construction method should specialize the codec rather than add an unsafe
blanket "any range is decodable" rule. Generic container decoding also requires its
element encoder, while an explicit codec can support a decode-only container.

Explicit codec operations and result encoding use `ConversionBudget`: aggregate element/byte limits
and balanced nesting-depth checks. Check limits before allocating/traversing; never
trust a claimed range size or call unbounded distance on an input range. Strings
charge bytes and decoded character work; nested elements charge recursively. Retain
temporary Terms in registered roots across allocating safe points. Native temporary
storage is owned per invocation, freed on every failure, and must not outlive its
caller context. A custom codec must honor the same ownership, budget and root rules.

## Calling and scheduler integration

String-based `resolve()` is thread-safe metadata lookup; atom-based lookup and load
must run on the supplied name Terms' owner thread. `prepare(context, arguments)` is owner-thread
work: select the registered signature/fallback, check context liveness and argument
ownership, then retain roots or move the exactly typed payload into a new frame.
It does not execute C++ or borrow the input span after return. New invocations get
separate frames and owned arguments; the immutable
binding alone is shared. Frames are tied to their originating context and reject
resumption under a different context.
After selecting a variant, validate caller ownership/liveness of Term roots retained
in its payload, including nested Term containers, without invoking argument decoders.
Selection itself compares only type keys. Caller-supplied native strings/numbers
are passed through as supplied; explicit codec validation policies do not influence
dispatch. Any function-specific value validation belongs to that selected function.

`CallFrame::resume(context, ticks)` is invoked by the containing process continuation
under its scheduler grant. `CallProgress` is either yielded/waiting or a completed
rooted Term; `CallResult` reports failure separately. A generated-code frame can yield,
wait for a mailbox and resume later. The containing `ProcessCode` propagates suspension
to the scheduler and consumes the returned value before continuing its caller.
Finishing a function does not exit the process unless it is the process's entry frame.
Calling resume on a completed/failed frame returns invalid_frame.

For a native frame, check that at least one tick is available before entering, then
charge one function-call unit. Pass through the selected typed values or Term roots,
invoke the target exactly once, encode its return into the caller's heap, and finish. A lack
of ticks yields before any body side effects. Conversion budgets bound adapter work;
they are not a timer or a guarantee that arbitrary user C++ executes within a tick.
Native targets must perform bounded synchronous work and must not block, await a
mailbox, wait on a future or retain their context/arguments. Long-running/asynchronous
extensions need their own cooperative `Callable`/`CallFrame` adapter. The eventual
resumable result encoder may yield between stages while retaining roots;
it must never re-execute an already-completed native body.

Arity, ownership and signature-selection errors prevent entry into the target.
`CallFailure` identifies
the zero-based bad argument and underlying TermError where applicable. Native C++
exceptions are caught as native_exception; allocation failures map to resource_limit.
No C++ exception crosses into generated code. Encoding a return can fail after the
body has performed side effects; those effects are not rolled back or retried. The
compiler/runtime boundary must translate failures into its Erlang exception protocol
(for example badarg/undef policy) when that protocol is implemented; this sketch does
not silently fabricate Erlang exception terms. Expected/error-returning C++ functions
can be wrapped explicitly or added through a reviewed codec/result policy later.

## Illustrative registration and lookup

These are declaration-only APIs; the following shape is syntax-checked, not linked:

```cpp
using namespace erlang_aot::runtime;

std::int64_t native_size(std::vector<std::int64_t> values);
Term generic_size(ProcessContext &context, Term value);

TermFactory terms(context);
auto module_name = terms.atom("native_demo"); // Check every TermResult before extracting values.
auto size_name = terms.atom("size");
ModuleDefinition module{
    .name = module_name.value(),
    .image = CodeImage::linked(),
    .exports = {
        {size_name.value(), std::make_shared<NativeCallable<std::int64_t(std::vector<std::int64_t>)>>(&native_size)},
        {size_name.value(), std::make_shared<NativeCallable<Term(Term)>>(&generic_size)},
    },
};
auto loaded = server.load(std::move(module));             // Check CodeResult.
auto resolved = server.resolve("native_demo", "size", 1); // Check CodeResult.
auto frame = resolved->prepare(context, argument_terms);  // Calls generic_size with original Terms.
auto typed = resolved->prepare_native(context, NativeArguments{std::vector<std::int64_t>{1, 2}});
// Exact vector<int64_t> registration calls native_size; no Term decoding occurs.
auto fallback = resolved->prepare_native(context, NativeArguments{std::list<std::int64_t>{1, 2}},
                                         argument_terms);
// No list<int64_t> registration: generic_size receives the explicitly supplied argument_terms.
// Check CallResults; the process continuation drives (*frame)->resume(context, ticks).
```

The original `argument_terms` span contains one caller-owned proper list for size/1.
Both size registrations have arity one; generic_size's context parameter is injected. Return
Terms must belong to the caller; use explicit heap copying before returning a value
from another accessible heap.

## Implementation and validation after review

Implement registration/resolution and lifecycle first under `runtime/src/code/`,
then exact-signature/fallback dispatch and native call frames. Keep explicit scalar/
sequence codec implementation separate from argument selection. Approved headers can
move to `runtime/include/erlang_aot/runtime/`. Connect generated-code adapters only
after their entry ABI, root maps and cooperative call protocol are established.
No general C++ object, std::function or STL type is itself an LLVM/C ABI signature.

Test same-MFA distinct signatures, duplicate signatures despite differing return types,
exact match precedence, integer/double mismatch, container-type mismatch, boxed values
selecting only all-Term, missing fallback, missing explicit fallback arguments, mixed
Term/native signatures, and zero-arity registration. Use instrumented codecs to prove
that matching/invocation never calls argument decoders or retries another body.
Test atomic failed registration, overloaded arities, missing/private exports, module
snapshots, concurrent resolve/unload and image lifetime after prepared-frame creation.
Check zero-arity/context injection, unsupported signatures, all integer limits,
float precision/non-finites, Unicode/NUL, nested/empty/improper lists, array length,
conversion caps, wrong owners, rooting, native exceptions and return-conversion
failures without repeating body side effects. Use a counted CodeImage destructor to
verify that active frames prevent early unmapping.

Validation passed on macOS arm64: standalone/combined C++23 syntax with warnings as
errors, accepted/rejected template shapes, exact-type/all-Term API usage, the
atom-based registration consumer and the documentation example,
clang-format, repository clang-tidy, Lizard and local documentation links. These are
structural checks; runtime behavior, threading, GC and platform loaders remain
unimplemented and require behavioral tests during implementation. No full-build gate
or commit was performed for this declaration-only sketch.
