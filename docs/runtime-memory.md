# Process memory ownership

Each `ProcessContext` owns a distinct stable `ProcessHeap`. Host operations are
serialized. Configuration and context creation validate nonzero byte budgets that
are multiples of the native target word, with chunk size no larger than the limit.
Configuration allocates storage metadata but no backing chunks.

`allocate(words)` returns exactly `words * sizeof(Word)` zeroed bytes. `reserve`
returns a move-only construction reservation with explicit commit and automatic
rollback. Requests reject zero, multiplication overflow, unsupported alignment,
overlapping reservations and exhausted capacity before publishing bytes. Alignment
must be a power of two between word alignment and `alignof(max_align_t)`.
Default budgets are 64 KiB chunks and 64 MiB total backing per process.

Chunks never move. Growth accounts for unused tails and alignment padding, caps
total capacity, and allocates a larger chunk when a single request needs one.
`used_words` includes consumed padding; `capacity_words` counts retained backing.
Rollback restores both counters and releases newly added chunks while preserving
earlier committed addresses. Allocation failure is `out_of_memory`; budget failure
is `limit_exceeded`. Active generated invocations receive the corresponding exact
checked infrastructure status. Allocation does not emit a deferred-feature report.

Constructors initialize reserved bytes before commit. Resource-bearing C++ objects
transfer a destructor callback at commit; failure to register the callback destroys
the initialized resource and rolls back. Final teardown runs committed callbacks in
reverse order before freeing chunks. Resources are never relocated as raw bytes.
One reservation may be outstanding per heap; nested construction must stage children
first and reserve the parent last. Runtime metadata is separate from backing-word
accounting; its allocation failures are contained and tested.

Storage ownership is independent of a context's address. Context teardown invalidates
its lifetime token first. An outstanding reservation pins storage for safe cleanup,
but denies byte access and commit after expiration. Factories already retain weak
lifetime tokens. Compound host admission remains gated pending step-12 lifetime
tests; existing host/error atoms retain immutable spelling pins. Immediate copies and
same-runtime atoms remain supported; foreign atoms reject instead of remapping.

Generated arguments, temporaries and result handoffs use
[revision-4 root scopes](generated-roots.md). `collect()` still reports
`not_implemented` without statistics or reclamation; no allocation retry invokes GC.
Graph copying, message transit ownership, mailbox/continuation roots and binary
storage are separate future work. Heap references and committed raw spans are
borrowed and cannot be used after their owner exits.

`runtime_memory` checks overflow, exact limits, stable growth, alignment, rollback,
resource destruction and expired reservations. `runtime_lifecycle_failure` sweeps
backing/chunk/resource/root allocations and checks cleanup and retry. Native
generated-call fault workflows test root scopes on success, errors and exceptions.
See [step-11 validation](patternmatch-step11-validation.md). Other native hosts and
moving-GC survival are not established by these tests.
