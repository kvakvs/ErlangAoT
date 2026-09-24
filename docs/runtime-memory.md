# Process memory ownership

Step 12 implements the memory service boundary in `runtime/src/memory/`.
Each live `ProcessContext` owns a distinct, noncopyable, nonmovable `ProcessHeap`
and mailbox. The runtime publishes a context only after construction succeeds;
failed construction releases its bookkeeping without affecting existing owners.
The [lifecycle contract](runtime-lifecycle.md) still requires host serialization.
Generated heap allocation, TermFactory, graph copying, root registration, stack
storage, binary allocation and garbage collection remain unsupported.

## Implemented operations and units

[ProcessHeap](../runtime/include/process_heap.hpp) takes allocation requests and
reports usage/capacity in target words. `HeapOptions` remains byte-based: both
budgets must be nonzero multiples of `sizeof(Word)`, with chunk size no larger
than the limit. `Word` comes from the runtime target, never the compiler host.
Configuration reserves policy only; even the largest valid budget allocates no
backing storage. A future successful `allocate(words)` will return a byte span of
exactly `words * sizeof(Word)` bytes aligned for `Word`.

| Operation | Current result |
|---|---|
| `allocate(0)` or a word count whose byte size overflows `size_t` | `HeapError::invalid_size` |
| Representable allocation larger than the configured byte limit | `HeapError::limit_exceeded` |
| Allocation within the budget | `HeapError::not_implemented` |
| `collect()` | `HeapError::not_implemented`, without statistics or a claimed safe point |
| `add(value)` / `value.copy_to(heap)` | Revalidated small integer, empty tuple or nil, with identical bits |
| Copy of the default invalid `Term` slot | `TermError::invalid_encoding` |
| `used_words()` / `capacity_words()` | Zero |

These operations are nonthrowing and allocate no host or process storage. Immediate
values have no owner or lifetime dependency: copies can cross runtimes and survive
both contexts' destruction. This exception does not extend to future identity or
heap values. `Term::from_word` continues to reject them; no tagged pointer is
constructed or dereferenced. Heap references themselves remain borrowed and cannot
be used after their context exits.

## Future allocation, roots and teardown

Allocation must check size arithmetic, alignment, backing capacity and resource
limits before publication. Future stable chunks must account for unused tails and
alignment padding, preserve existing addresses during growth, and respect any
stronger native alignment required by C++ members. Backing allocation failure will
return `HeapError::out_of_memory`; graph-copy budget/allocation failure will map to
`TermError::resource_limit`. No successful empty allocation or implicit fatal exit
is substituted for these errors. Erlang exit policy belongs to a future execution
adapter. Runtime/context bookkeeping already contains allocation failures as
`Status::out_of_memory` and cleans up partial construction.

Before heap-valued Terms become constructible, external metadata must register
host handles and retain/check `ContextLifetime`. One-word term slots alone cannot
serve as lifetime-checked host roots. Factories, temporary graph copies, generated
locals and saved continuations need explicit root registration and unwind rules.
The collector must also trace mailbox terms and active receive candidates. A
registered owner safe point must precede tracing; a future unsafe call returns
`HeapError::unsafe_point`. This skeleton has no safe-point registry and always
reports collection as unavailable.

Pending message signals must own independent transit data, including self-sends;
they cannot borrow a sender heap. Only receiver-side handling imports the payload
and installs mailbox roots. Failed import must not publish a partial message.
Transit data lives outside either process heap and requires its own budget and
cleanup. These are contracts for future signals and receive, not implemented paths.

Context destruction invalidates its lifetime token before mailbox and then heap
teardown. Future execution must first cancel waiters, release continuation/cursor
roots and discard pending signals. Cells containing C++ resources need explicit
construction and exactly-once destruction before backing storage is released.
The private [layout assertions](../runtime/src/terms/term_layout.hpp) retain native
word sizes/alignment and distinguish immediate slots from nontrivial resources.
Tracing follows term slots only; numeric bytes, registry IDs, padding, Boost
internals and smart-pointer representations are not roots.

## Shared binary reservation

[BinaryHeapObject](../runtime/include/binary_heap_object.hpp) remains a declaration
sketch. Future creation makes a checked immutable copy into its own `vector<Word>`;
the word count must be strictly greater than `HEAP_BINARY_THRESHOLD_WORDS`
(`64 / sizeof(Word)`). Smaller payloads belong on process heaps. A zero tail count
means a full final word; otherwise 1 through `ERL_WORD_BITS - 1` counts valid high
bits. Exact length is `(word_count - 1) * ERL_WORD_BITS + valid_last_word_bits`,
with checked arithmetic and unused low bits cleared. Byte-aligned partial words
are valid binaries; other lengths are bitstrings.

Published vector addresses stay stable while a shared owner exists. Copying an
immutable shared payload can retain a shared owner while copying its process-local
wrapper. Collection/exit must destroy shared handles; relocation must use proper
C++ lifetime operations, never byte-copy `shared_ptr` representations. The final
shared owner releases the vector directly, without a binary pool, separate heap
or callback. Allocation and integration with term layouts remain deferred.

## Validation

`runtime_memory` exercises separate process owners/budgets, size overflow, limits,
unsupported allocation/collection, invalid slots, immediate copy equivalence and
survival after source/destination teardown. `runtime_lifecycle_failure` sweeps
construction allocation failures and exercises memory boundaries with host
allocation forced to fail. Native layout tests compile the prefix assertions.
Sanitizer runs cover these boundaries; they do not validate an allocator, collector,
heap graph or message implementation. Foreign native runtime runs remain pending.
