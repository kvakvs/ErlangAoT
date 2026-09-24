# Runtime term API — manual review sketch

Status: immediate ABI defined in compilation step 7, 2026-09-24. Heap allocation,
term services, rooting and collection remain proposed and unimplemented.
[terms.hpp](../include/terms.hpp) preserves the one-word public value API;
[term_layout.hpp](../include/term_layout.hpp) contains compile-checked private prefixes.
These headers are listed for IDE navigation and compiled by focused tests.
[process_heap.hpp](../include/process_heap.hpp) reserves process storage and graph copying.
[atom_storage.hpp](../include/atom_storage.hpp) and [atom_storage.md](atom_storage.md)
reserve runtime-wide interning. Compiled metadata will record atom spellings, never
compiler-assigned IDs; runtime binding and atom-valued expressions are later steps.

## Class boundary and immediate ABI

`Term` remains a final common value API with private storage and process-bound
`TermFactory` construction. Its sole `Word` stores an immediate or a future tagged
heap pointer. Public checked accessor declarations are preserved. The default zero
word is an invalid/uninitialized slot, not nil or a valid boxed value. Root/owner
tracking will require external metadata and explicit safepoints; a one-word value
cannot itself contain a smart-pointer lifetime token. Those services are not yet
implemented and the eventual root design must be validated before heap lowering.

The implemented private/versioned contract is [ABI v1](../../abi/include/erlang_aot/abi/v1.h)
with checked C++ integer helpers in [term.hpp](../../abi/include/erlang_aot/abi/term.hpp).
A generated function uses the platform C calling convention and returns an unsigned
pointer-width term, accepting an opaque live context and a borrowed term-array
pointer. Arity belongs to the resolved identity; a zero-arity array may be null.
The context propagates unchanged through direct calls. C++ `Term`, STL values,
`std::expected` and exceptions never cross that boundary. There is no BEAM/FFI
compatibility promise or public heap ABI. Step 9 implements
[runtime lifecycle](../../docs/runtime-lifecycle.md) independently of term services.

Tags are numerical low bits, decoded with masks/shifts rather than C++ bitfields
or inactive union members. Primary bits 0–1 reserve header=0, list=1, boxed=2 and
secondary=3. Bits 2–3 then select pid=0, port=1, tertiary=2 or small integer=3.
Tertiary bits 4–5 reserve atom=0, catch=1, empty tuple=2 and nil=3. Only small integer
encoding is implemented: `(unsigned(value) << 4) | 0xf`, after checking the exact
signed range `[-2^(word_bits-5), 2^(word_bits-5)-1]`. Negative decoding explicitly
reconstructs the signed payload without implementation-defined unsigned-to-signed
conversion or signed right shift. No heap pointer encoder is provided yet.

Both 32-bit and 64-bit codecs are tested on every host. Cross compilation takes
width/alignment from the LLVM target layout, never host `sizeof(Word)`. The native
C header, C++ layouts and LLVM term/signature types agree on size and alignment.
Other native runtime toolchains still need their own full layout validation.

## Explicit process-heap layout

`Word` is the runtime target's unsigned pointer-width type, aligned to 4 or 8 bytes.
`Term`, `TermTag` and `BoxHeader` are each one word. The private header reserves low
two bits 00, five kind bits at bits 2–6, and a content-word count starting at bit 7.
That count excludes the header and includes all remaining prefix, trailing payload
and allocation padding. Checked header construction remains future heap work.
Cons cells have no header; they contain exactly a head term and a tail term.

| Private prefix | Fixed size | Trailing storage / future tracing |
|---|---|---|
| `BignumCell` | Native C++ layout | Owned Boost value; no term slots; may need alignment stronger than a word |
| `FloatCell` | One word + 8 bytes | IEEE binary64 bytes, no traced slots |
| `RemoteIdentityCell` | 3 words | Registry ID untraced, remote-host term traced |
| `ConsCell` | 2 words | Head and tail traced |
| `TupleCell` | 1 word | Arity consecutive term slots |
| `MapCell` | 1 word | Key/value term pairs |
| `HeapBinaryCell` | 2 words | Untraced Word data; valid high tail bits, zero means full final word |
| `RefcBinaryCell` | Native C++ layout | Shared `BinaryHeapObject`, released by explicit C++ destruction |
| `ExternalFunctionCell` | 4 words | Module/name terms traced; arity untraced |
| `ClosureCell` | Native C++ layout | Reserved callable weak reference and count, followed by traced capture slots |
| `NativeRecordPrefix` | 3 words | Descriptor ID/count followed by traced field slots |

All variable payloads follow fixed prefixes as separately allocated storage, without
flexible arrays or fake `[1]` members. Future allocation must check count arithmetic,
word rounding and budgets, respect each prefix's alignment, and establish the C++
lifetimes of both prefixes and trailing elements before access. Native C++ members
need explicit construction/destruction and cannot be serialized or moved by blind
byte copying. No constructors perform writes into unallocated trailing storage.

A future collector must trace only term slots, rewrite relocated pointers, and avoid
interpreting numeric bytes, registry IDs, smart pointers or Boost internals as terms.
Raw heap pointers cannot survive safepoints without re-resolution through roots.
Runtime IDs remain stable while referenced, and code references require pinning.
These are private reservations, not implemented GC, identity or callable services.

## Coverage and construction

The categories follow the [OTP data-type reference](https://www.erlang.org/doc/system/data_types.html).
Native records are a distinct, experimental OTP 29 category; traditional records
remain tuples. Booleans are atoms and strings can be integer lists; neither needs
a separate storage kind. Binaries are bitstrings with a bit count divisible by eight.

| Category                             | Creation                                                         | Inspection / extraction                                                                          | Functional changes                                               |
| ------------------------------------ | ---------------------------------------------------------------- | ------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------- |
| Integer, including bignum            | `integer`, `integer_decimal`                                     | `is_integer`, checked `integer_value`, lossless `integer_decimal`                                | Construct a replacement                                          |
| Float                                | `floating`                                                       | `is_float`, `float_value`                                                                        | Construct a replacement                                          |
| Atom / boolean                       | `atom`, `boolean`                                                | `is_atom`, `is_boolean`, `atom_utf8`, `atom_id`, `boolean_value`                                 | Construct a replacement                                          |
| Nil / cons / proper or improper list | `nil`, `cons`, `list`                                            | `is_nil`, `is_cons`, `is_list`, `is_proper_list`, `head`, `tail`, `list_length`, `list_elements` | `prepend`, `append`, `with_list_element`                         |
| Tuple                                | `tuple`                                                          | `is_tuple`, `tuple_size`, `tuple_element`, `tuple_elements`                                      | `with_tuple_element`                                             |
| Map                                  | `map`                                                            | `is_map`, `map_size`, `map_contains`, `map_find`, `map_entries`                                  | `with_map_entry`, `with_existing_map_entry`, `without_map_entry` |
| Bitstring / binary                   | `bitstring`, `binary`                                            | `is_bitstring`, `is_binary`, `bit_size`, copied bytes                                            | `bit_slice`, `concat_bits`                                       |
| Pid / port / reference               | Wrap issued identity; `make_reference` creates a fresh reference | Corresponding predicate and opaque identity accessor                                             | Identities are immutable                                         |
| Function                             | `external_function`, `closure`, `function`                       | `is_function`, arity predicate, `function_arity`, opaque identity                                | Construct a replacement closure                                  |
| Native record                        | `native_record`                                                  | Category/descriptor predicates, descriptor and named fields                                      | `with_record_field`                                              |

`ProcessContext` and `ProcessIdentity` are forward-declared here and sketched in
[process.hpp](../include/process.hpp). Other identity classes, closure and record descriptors
remain forward declarations for their owning subsystems. Their complete types are
required before calling corresponding `expected<T, ...>` accessors. None of these
declarations represents an implemented process, port, module or scheduler API.

Runtime services issue process/port/reference identities; wrapping them does not
spawn a process, open a port or forge an identity. The module service issues closure
and native-record descriptors; callers cannot supply executable addresses.
External funs retain module/name/arity for later resolution and need not resolve
at construction. Closure descriptors validate capture count and owner; function
identities retain captures. Invocation and scheduler integration are outside this API.
Distribution and serialization, including imports of remote identities, are deferred.

## Values, ownership and errors

The following are proposed service contracts; step 7 implements only raw integer encoding.
Step 9 supplies `ProcessContext::lifetime()`: host binding/root metadata will lock or
retain its `ContextLifetime` token and check liveness before touching the context.
Destruction clears liveness before mailbox/heap release; retaining the token does
not retain that storage. The one-word `Term` still needs external root/owner metadata.

- Factories bind to one process context. Every returned term is a rooted host
  handle; copying retains the value, assigning rebinds only that C++ handle.
  The current sketch permits an invalid default slot; it must be initialized before use.
  Moved-from handles support only
  destruction or assignment; all inspection requires a live handle.
- Input spans and strings are borrowed only for the duration of a call. Extraction
  returns owned text/bytes, copied opaque identities or rooted child handles.
  No borrowed view survives heap movement or collection.
- Composite construction and updates accept terms from the same process context.
  Cross-context inputs return `wrong_owner`, even for immediates, to keep one
  predictable contract. `copy_to`/`ProcessHeap::add` are explicit cross-heap operations;
  ordinary C++ handle copies never transfer process ownership.
  Runtime-issued identities/descriptors must belong to the same runtime instance.
- Scheduler-mediated process exit invalidates term/factory lifetime tokens before
  freeing heap storage; surviving handles report `expired_context` for checked
  operations. Unchecked predicates require a live context. Identity values retain
  runtime identity independently of process liveness; no raw context pointer may dangle.
  Handles are confined to their owning process execution thread until a scheduler
  handoff contract exists; smart-pointer ownership does not imply thread safety.
- Semantic failures use `TermResult<T>`. Wrong accessors return `wrong_type`;
  narrowing/index/slice errors return `out_of_range`. Invalid atoms, decimal text
  and bit encodings are rejected. Allocation/traversal budgets return
  `resource_limit`; later unimplemented operations return `not_implemented`.
  There are no success-shaped stub definitions in this sketch.
- C++ bookkeeping allocation may throw `std::bad_alloc`, including handle copies;
  no general `noexcept` promise is made. The future ABI adapter must catch and
  translate host exceptions. Term errors are not automatically Erlang exceptions;
  the BIF/compiler boundary chooses the appropriate Erlang failure behavior.
- `exactly_equal` compares Erlang values, including arbitrary integers and identity
  terms. Map keys use exact equality (integer `1` and float `1.0` are distinct).
  There is no pointer-based equality operator or exposed hashing policy.

## Heap ownership, copying and collection

`ProcessContext` owns one `ProcessHeap` as an explicit member, and `TermFactory`
allocates and registers roots there. The heap's `add(value)` and
`value.copy_to(destination)` describe the same operation: copy the reachable term
graph into the destination and return a destination-owned rooted handle. This
includes container children and closure captures, integer limbs and binary data.
Preserve sharing within the copied graph using a visited map; never retain source
heap pointers. Heap-resident cells are copied even for a same-heap request; immediate
values and immutable runtime-wide atom/identity/descriptor entries need no duplicate
registry allocation. Copying a pid/reference/fun preserves identity, not liveness.

Both heaps must belong to the same runtime, be live, and be exclusively accessible
to the caller on their owner thread. Different heaps assigned to the same scheduler
can be copied directly while the source is rooted and quiescent. Calling from one
worker into another worker's heap is forbidden (`wrong_owner`); messaging uses
independently owned transit storage, then receiver-side import instead. Cross-runtime
and remote serialization remain outside this copy API. An expired source or
destination reports `expired_context`; allocation/budget exhaustion reports
`resource_limit` (host bookkeeping allocation may still throw `std::bad_alloc`).

Copying registers temporary source/destination roots before any allocating safe
point and publishes the result only after success. Failure must release those
temporary roots without exposing a partial value or changing the source. Unreachable
partial allocations can remain charged to the destination until GC/exit; successful
copying does not promise rollback of backing capacity. Work/size limits must bound
graph traversal. Tests must verify all term categories and that a copied value stays
usable after the source process exits.

`ProcessHeap::collect()` is the explicit GC boundary. Only the owner at a registered
safe point may collect; otherwise return `HeapError::unsafe_point`. The first
non-collecting implementation reports `not_implemented`, never fabricated reclamation
statistics. The future collector enumerates host roots, saved continuation roots,
mailbox terms and active receive candidates, traces the private layout, releases
unreachable cells/resources and rewrites moved slots. Heap growth alone keeps addresses
stable. Raw `allocate()` spans are runtime-internal construction borrows: publish/root
the completed cell before any collection safe point; partially initialized cells
must not be scanned. Compiler locals must have root maps before collection is enabled.

## Operation details to review

- Updates are **persistent**: each returns a new term; existing aliases retain
  their values. Private sharing/copy-on-write is permitted only when unobservable.
  Builders and mutable references are not part of this proposal.
- C++ indices are zero-based. An Erlang `element/2` or `setelement/3` adapter must
  validate/convert the language's one-based index at its boundary.
- `cons(head, tail)` accepts any tail and represents improper lists. `is_list`
  tests nil/cons like the Erlang BIF; `is_proper_list` traverses to verify the final
  tail is nil (returns false for nonlists). `prepend` accepts nil/cons, including
  improper lists. `append` adds **one element** at the end of a proper list and
  may copy its spine. Length, bulk extraction and replacement require proper
  lists; nonlist receivers return `wrong_type`, improper tails `improper_list`.
  Traversals must be iterative and bounded, without exposing cyclic term creation.
- Tuple replacement preserves arity; an empty tuple is valid. List/tuple bulk
  extraction can be expensive and is subject to resource budgets.
- Map construction consumes entries in order, with the last duplicate key winning.
  Insert replaces or adds, update-existing reports `missing_key`, removal of an
  absent key succeeds unchanged. Lookup distinguishes missing from any stored
  term. Enumeration order is unspecified and must not expose the backing container.
- Decimal integer input accepts an optional sign and at least one ASCII digit,
  without whitespace, separators or a size-limited host conversion. Output is
  canonical decimal (no leading plus/zeros, zero is `0`). Floats require finite
  values; integer access never silently coerces a float. Atoms validate UTF-8,
  preserve spelling without Unicode normalization, and enforce OTP-compatible
  length limits plus runtime AtomStorage budgets. New names receive sequential
  runtime-local IDs wrapped as atom Terms; existing names retain their IDs.
  The startup cap defaults to 2^20 entries and cannot exceed the hard limit of 2^26.
- Bitstrings use exactly `ceil(bit_count / 8)` bytes with MSB-first significant
  bits; unused low bits in the last byte must be zero on input and output. Empty
  bitstrings/binaries are valid. Slices use bit offsets/counts and checked bounds.
  `binary_bytes` rejects non-byte-sized values; no storage-alignment promise leaks.
- Native-record construction requires all fields in registered descriptor order;
  defaults are supplied by lowering before construction. Named access/update
  takes an atom; an unknown field reports `unknown_field`. Descriptor identity,
  visibility and compatibility must follow the selected OTP 29 contract when
  implemented, not be approximated by a tuple tag.

## Example flow (illustrative; not executable)

With `TermFactory terms(context)`, create `terms.integer(42)` and
`terms.atom("answer")`; check each `TermResult` before extracting its value.
Construct a list from an array/span of terms, then use `list.append(value)` to
grow it. Construct a tuple and call `tuple.with_tuple_element(0, replacement)`;
construct a map and call `map.with_map_entry(key, replacement)`. Each operation
produces a separate result, and the original list/tuple/map remains unchanged.
Call `is_integer()` before a bounded `integer_value()` when convenient; the
accessor remains checked even if the predicate was omitted. Bignums can instead
be extracted losslessly through `integer_decimal()`.

## Review decisions and later validation

Review the explicit heap prefixes/slot format and tracing table, the unified
public `Term` versus typed wrapper classes, persistent update
names, process confinement/root lifetime, opaque identity ownership, explicit
errors with host allocation exceptions, and whether bulk extraction is sufficient
before adding iterators. Immediate tag allocation is fixed by ABI v1; GC/root machinery, heap services
and runtime bridging remain implementation decisions.

After approval, implement in small steps under `runtime/src/terms/` and move the
approved API declarations into `runtime/include/erlang_aot/runtime/`; keep heap
structs private under `runtime/src/terms/`. Define all opaque
dependencies and review their lifetimes before enabling identity/descriptor APIs.
Keep unsupported capabilities explicit; a declaration does not expand the compiler
subset. Future tests must cover all predicates, bignum/narrowing boundaries,
improper tails, empty containers, alias preservation, map exact-key semantics,
bit edges, descriptor validation, wrong owners, roots across GC and failure cleanup.
Add cross-heap graph-copy coverage, source-exit independence, partial-copy failures,
mailbox/cursor roots and collection-safe-point rejection.
Validate prefix/array alignment, allocation overflow, scanner coverage and layout
assertions on 32-bit and 64-bit target builds before enabling heap code generation.
Step 7 compiles all prefix assertions on native macOS arm64 and tests the complete
64-way tag truth table, immediate integer boundaries/overflow for both widths,
and target-derived LLVM layouts/signatures. These checks do not establish heap
allocation, ownership, collection or native foreign-platform runtime correctness.
