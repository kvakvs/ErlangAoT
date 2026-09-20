# Runtime term API — manual review sketch

Status: proposed, 2026-09-20. **No runtime implementation.**
[terms.hpp](terms.hpp) contains API declarations only;
[term_layout.hpp](term_layout.hpp) sketches private heap structs and layout assertions.
[process_heap.hpp](process_heap.hpp) owns process term storage and declares addition,
cross-heap copying and collection. All sketches are deliberately outside the
exported include tree and all CMake targets.
[atom_storage.hpp](atom_storage.hpp) and [atom_storage.md](atom_storage.md) define
runtime-wide interning, stable numeric atom IDs, startup caps and the atom-GC placeholder.
Compiled atom literals are read-only constants initialized through AtomStorage at
runtime; the compiler records spellings/constant slots and never assigns numeric IDs.
Nothing here can be linked yet;
this is not completion of a step in the [compilation plan](../../.agents/04-compile.md).

## Class boundary and representation

`Term` is the common parent **value API** for all categories. The proposed public
classes are `Term` and process-bound `TermFactory`; category-specific payloads
are private structs with controlled memory layout. This deliberately proposes one final
public value class rather than a public `IntegerTerm : Term` hierarchy. Review
this choice explicitly: consumers use checked accessors rather than downcasts,
and replacing boxed scalar structs with tagged words leaves their source API
unchanged. If public typed wrappers are wanted, they can be added as checked
views without exposing payload inheritance or storage.

The forward-declared `Term::Impl` is an external host root handle, not a heap
payload or a polymorphic term tree. It hides representation, allocation, roots and
ownership. The sketch's private smart pointer retains that host root only; it is
not stored in heap cells and does not mandate allocating every future integer.
No public tag bits, integer limbs, pointers to
heap cells, STL container references or mutable payload references are exposed.
Changing the C++ object layout may require rebuilding C++ consumers; this is a
source-compatibility goal, not a stable C++ binary ABI promise.

The generated-code ABI remains a separate private/versioned contract in `abi/`.
The plan's unsigned word representation can coexist with these host handles.
Only a runtime-private bridge will box/unbox and register roots. Neither these
C++ classes nor `std::expected`, STL types or C++ exceptions cross that C boundary.
The initial executable subset can still implement only immediate integers.

## Explicit process-heap layout

The heap proposal is a word-aligned arena of **plain structs**, not C++ objects
with virtual dispatch, inheritance, smart pointers or container members. `Word`
is the runtime target's unsigned pointer-width integer (32 or 64 bits).
`TermSlot` is exactly one word. All object prefixes start with a two-word
`Header { kind, size_words }`. `size_words` includes the prefix, trailing payload
and word-rounding padding. The draft asserts field offsets and sizes instead of
depending on packing pragmas or implementation-defined bitfields. GC mark and
forwarding state use side metadata in this proposal, keeping the payload layout
simple until a collector is chosen.

| Private struct | Fixed words | Trailing payload | Slots traced by a future GC |
| --- | --- | --- | --- |
| `NilCell` | 2 | None | None |
| `IntegerPrefix` | 4 | `limb_count` unsigned words | None |
| `FloatCell` | 2 + 8 / word bytes | None | None |
| `AtomCell` | 3 | None | None; runtime table ID |
| `IdentityCell` (pid/port/reference) | 3 | None | None; runtime registry ID |
| `ConsCell` | 4 | None | Head and tail |
| `TuplePrefix` | 3 | `arity` slots | Every element |
| `MapPrefix` | 3 | `count` pairs of slots | Every key and value |
| `BitstringPrefix` | 3 | `ceil(bit_count / 8)` bytes, padded | None |
| `ExternalFunctionCell` | 5 | None | Module and name atoms |
| `ClosurePrefix` | 5 | `capture_count` slots | Every capture |
| `NativeRecordPrefix` | 4 | `field_count` slots | Every field |

For example, a cons is `header | head-slot | tail-slot`, a tuple is
`header | arity | element-slots...`, and an integer is
`header | negative | limb-count | magnitude-limbs...`. Fields are addressed by
the checked offsets in the draft. Trailing arrays start at `sizeof(Prefix)`;
they are separate arena storage, **not** a C++ flexible array or a fake `[1]`
member accessed out of bounds. Allocation must check count multiplication,
addition, word rounding and budgets before reserving memory. The implementation
must establish the C++ lifetime of prefixes and trailing arrays correctly, and
never read padding/uninitialized slots as values. There is no allocator or
pointer arithmetic implementation in this change.

Initially a valid slot contains the address of a boxed, word-aligned cell;
zero is invalid, so nil has its own cell. Every link, including an element inside
a container and a host root, uses this same slot format. Future small integers
can use low tag bits without changing container layouts or the public API;
32-bit targets guarantee fewer alignment bits than 64-bit targets. Exact masks,
payload range and tag allocation remain a versioned ABI decision. The plan's
immediate-integer milestone may introduce those tags directly rather than first
implementing the all-boxed representation. These proposed layouts are private,
not BEAM-compatible, serializable or fixed across ABI revisions.

A moving collector must rewrite all rooted and heap-contained slots that point
to moved cells; it must not reinterpret integer limbs, float bits, packed bytes
or registry IDs as references. Internal C++ cell pointers cannot survive a
safepoint without re-resolution through a root. Atom/identity/descriptor IDs are
stable for the runtime instance and cannot be recycled while values could refer
to them. Initially tables retain entries until runtime shutdown and fail at their
resource limit. The atom-storage sketch reserves future reclamation/compaction of
unreachable atoms without renumbering survivors or reusing IDs; other registry
reclamation remains deferred. Closure descriptors retain their code metadata.
Process termination does not erase the identity of an existing pid term. Future
table reclamation needs its own reachability and generation contract.

Integer magnitude limbs use target-native words, least significant first, with
no leading zero limb. A zero integer has zero limbs and `negative == 0`; other
signs are 0/1. Floats copy IEEE binary64 bits into byte storage so a 32-bit
target's `double` alignment cannot introduce hidden heap padding. Byte order is
target-native, not a cross-platform wire format. Maps start as flat unique-key
entries; replacing them with trees or hashes is private. Binaries start inline;
off-heap backing storage and sub-binaries need separately reviewed ownership.

Cross compilation must derive this contract from the **target** word size/data
layout. A host-side `sizeof` of these structs is not evidence for a different
target. Build the assertions with each actual target runtime toolchain; test
generated LLVM access against the same target contract before emitting heap ops.

## Coverage and construction

The categories follow the [OTP data-type reference](https://www.erlang.org/doc/system/data_types.html).
Native records are a distinct, experimental OTP 29 category; traditional records
remain tuples. Booleans are atoms and strings can be integer lists; neither needs
a separate storage kind. Binaries are bitstrings with a bit count divisible by eight.

| Category | Creation | Inspection / extraction | Functional changes |
| --- | --- | --- | --- |
| Integer, including bignum | `integer`, `integer_decimal` | `is_integer`, checked `integer_value`, lossless `integer_decimal` | Construct a replacement |
| Float | `floating` | `is_float`, `float_value` | Construct a replacement |
| Atom / boolean | `atom`, `boolean` | `is_atom`, `is_boolean`, `atom_utf8`, `atom_id`, `boolean_value` | Construct a replacement |
| Nil / cons / proper or improper list | `nil`, `cons`, `list` | `is_nil`, `is_cons`, `is_list`, `is_proper_list`, `head`, `tail`, `list_length`, `list_elements` | `prepend`, `append`, `with_list_element` |
| Tuple | `tuple` | `is_tuple`, `tuple_size`, `tuple_element`, `tuple_elements` | `with_tuple_element` |
| Map | `map` | `is_map`, `map_size`, `map_contains`, `map_find`, `map_entries` | `with_map_entry`, `with_existing_map_entry`, `without_map_entry` |
| Bitstring / binary | `bitstring`, `binary` | `is_bitstring`, `is_binary`, `bit_size`, copied bytes | `bit_slice`, `concat_bits` |
| Pid / port / reference | Wrap issued identity; `make_reference` creates a fresh reference | Corresponding predicate and opaque identity accessor | Identities are immutable |
| Function | `external_function`, `closure`, `function` | `is_function`, arity predicate, `function_arity`, opaque identity | Construct a replacement closure |
| Native record | `native_record` | Category/descriptor predicates, descriptor and named fields | `with_record_field` |

`ProcessContext` and `ProcessIdentity` are forward-declared here and sketched in
[process.hpp](process.hpp). Other identity classes, closure and record descriptors
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

- Factories bind to one process context. Every returned term is a rooted host
  handle; copying retains the value, assigning rebinds only that C++ handle.
  No default/invalid term is constructible. Moved-from handles support only
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
before adding iterators. Tag allocation, GC/root machinery,
integer backend and C ABI bridging remain implementation decisions.

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
No behavior tests or implementation are added by this review-only change.

Sketch validation: both headers pass a combined C++23 syntax check with warnings
as errors on macOS arm64, clang-format, the repository clang-tidy configuration,
Lizard (no function bodies) and whitespace checks. Layout assertions were evaluated
only for that native 64-bit target; 32-bit and other platform layouts are not yet
validated. No full build/behavior gate or commit was performed for this draft.
