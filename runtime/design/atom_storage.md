# Atom storage

Pattern/guard step 3 implements runtime-owned storage, validated UTF-8 spellings,
limits, deduplication, host ownership and module bindings. The authoritative contract
is [runtime atoms](../../docs/terms.md#atoms); the public project API is
[atoms.hpp](../include/erlang_aot/runtime/atoms.hpp).

This supersedes the 2026-09-20 review sketch. In particular, IDs use a process-wide
non-recycled namespace to reject foreign raw words; they are not dense per runtime.
Tables use spelling/word maps with immutable shared records behind one shared mutex
(plan 11 step 54, [threads](../../docs/runtime.md#threads)), not the proposed dense vector/hash table or a collector lock. Failed reservations
may consume IDs, while failed insertion leaves both indexes unchanged. No collector,
compaction, scheduler workers or launcher option parser is implemented.

## Deferred collection constraints

Atom collection needs a runtime-wide safe point and complete root enumeration:
process heaps/continuations, mailbox/receive/transit data, host Terms, pinned code
names/literals, descriptor registries, builtins and native metadata. Process-heap
collection alone cannot prove an atom unreachable. Table indexes are storage,
not semantic roots; no reclamation may run before every root category and worker
handshake exists.

Collection must preserve every survivor's exact word and spelling bytes, never
renumber or recycle words, and publish both indexes transactionally. Reinterning
a collected spelling requires a fresh identity. Reclamation may free table capacity
but cannot restore exhausted word identities. Concurrent creators, root/collection
races and retained values across compaction still need behavioral validation.
These constraints do not provide a collector or authorize a successful GC result.

## Historical review validation (2026-09-20)

The original declaration-only review recorded: standalone/combined C++23 syntax
with warnings as errors, option/API type assertions, clang-format, repository
clang-tidy, Lizard and local documentation links passed on macOS arm64. Lizard had
no function bodies to measure. That draft claimed no runtime behavior, full-build
gate, other-platform validation or commit. Current implementation evidence is
[the step-3 record](../../docs/validation.md#history).
