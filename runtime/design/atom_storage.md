# Atom storage — API for manual review

Status: proposed, 2026-09-20. [atom_storage.hpp](atom_storage.hpp) contains declarations
and option constants only. No table, lookup, interning algorithm, collector, command-line
parser or alternative lookup representation is implemented or added to CMake.

An Atom is a Erlang data type represented by a hidden constant integer value assigned at
atom creation, and a constant string, atom's name, also assigned at creation. Since creation
atoms can be used in the program and internally their numerical value is passed, tagged
as Atom data type. An Atom hidden integer value can never leave an Erlang node, they
always are converted to a string first to find the new numerical value on the remote
host or the future host which will read and instantiate this atom value.

## Runtime ownership and startup limits

Each runtime owns one `AtomStorage`, shared by all its processes, scheduler workers
and code services. `ProcessContext::atom_storage()` refers to that instance. This is
a singleton **within a runtime**, not a process-global static singleton: independent
runtimes in one host must not accidentally share IDs, atom roots or startup limits.
The runtime creates storage before interning builtin/module atoms and releases it
after stopping workers and releasing code metadata and all atom roots.

`AtomStorage::start(AtomStorageOptions)` is the startup configuration boundary:

| Option                 | Value                              |
| ---------------------- | ---------------------------------- |
| Default `max_atoms`    | 2^20 = 1,048,576 retained entries  |
| Hardcoded maximum      | 2^26 = 67,108,864 retained entries |
| Accepted startup range | 0 through 2^26-1, inclusive        |

Out-of-range caps fail with invalid_options; do not silently clamp them. Startup
may select a lower or higher cap within that range. All entries, including runtime
builtins, count toward the cap; bootstrap fails if its atoms cannot fit. This is a
runtime startup option, not an Erlang compiler setting. Its eventual launcher flag
or configuration parser is deferred; the C++ option is the API being reviewed.
The cap is immutable after startup. Storage grows on demand rather than reserving
the default or maximum table and string capacity upfront.

## Creation, numbering and both indexes

`create(context, spelling)` accepts a string view of UTF-8 bytes and returns a
`TermKind::atom` Term rooted in the supplied caller context. Its atom payload is a
monotonically allocated integer `AtomId`, not a pointer and not a `TermKind::integer`
Term. The context parameter provides ownership/root registration; the text determines
identity. `TermFactory::atom(text)` delegates here. `boolean()` interns/reuses the
same true/false atoms; it must not allocate identities in another registry.

Use the existing term atom validation contract: valid Unicode/UTF-8 and the selected
atom-name length limit, preserving exact spelling without normalization or case folding.
The input view is borrowed only during the call; storage owns the registered text.
Equality checks length and all bytes, including any embedded zero accepted by the
atom-name contract. Hash collisions never imply equal names.

Before GC exists, private storage has exactly two lookup structures:

1. A dynamically growing dense ID table of immutable `(id, spelling)` entries.
   IDs start at zero; successful new names receive 0, 1, 2, ... in publication order.
   Numeric lookup checks bounds and indexes `entries[id]` directly. Growth may move
   backing storage, but no API exposes an entry pointer or borrowed spelling view.
2. A name-to-ID hash index for deduplication and lookup by exact spelling. It may
   refer to owned string storage by stable internal offsets or own keys; it must
   never hold dangling views into a relocated backing allocation.

An existing name returns its existing ID, even at capacity. Only a new successfully
published name advances the counter. Registering the same name does not allocate a
second atom. Both indexes must publish together: validation, capacity failure or
allocation failure must leave neither a partial entry nor a consumed ID. Reserve
the entry, index and returned root's required resources before committing publication,
so pre-GC successful IDs remain dense with no gaps. Concurrent same-name creation
must return one identity; different new names are numbered in commit order.

The implementation will serialize publication under a storage lock and coordinate
lookups/root registration with collection. Owner-thread validation still applies to
the caller's process context. `find` and `lookup` atomically retain a root before a
future collector could reclaim the found entry. Do not invoke allocating process
code or arbitrary callbacks while holding an atom-table lock; reserve resources and
revalidate before the short publication/root-attachment step. The detailed GC/lock
protocol remains implementation work, with a defined lock order required before use.

## Compiled atom constants

The compiler represents each atom literal as a **read-only constant initialized by
a runtime call to AtomStorage**. Compilation fixes its spelling and constant slot,
but never assigns or embeds an AtomId. The numeric identity is obtained from the
executing runtime's atom table, so it may differ between runtime instances or runs.

Generated module initialization interns the emitted spellings through AtomStorage
(the reviewed `create(context, spelling)` API, or its eventual generated-code ABI
bridge). It fills the module's private constant bindings and registers their atom
IDs as runtime metadata roots before publishing any callable code. A temporary
initialization context may supply the initial Term roots; ownership must transfer
to module metadata before that context is released. This does not make arbitrary
process-owned Term handles safe to store globally.

Once initialized, atom bindings are read-only for the lifetime of that module in
that runtime. Generated function bodies read the initialized atom constants rather
than interning the spelling on every evaluation. Duplicate spellings, including
ones in different modules, resolve to the same runtime atom identity. A shared code
image uses separate constant bindings for each runtime, never one global ID patched
by whichever runtime happens to initialize it first.

Initialization is explicit and must finish before module publication/execution.
Failure leaves the module unpublished and releases its temporary/module roots;
successfully interned entries can remain in AtomStorage under the existing no-GC
policy. Initializer-private storage is writable while being filled; only completed
bindings are exposed as read-only. Do not write a runtime-assigned ID into storage
that is already immutable or pretend it was a compile-time numeric constant.

Pinned module instances/call frames retain the constant roots until their code can
no longer execute. Future atom GC may relocate atom storage, but neither the
compiled constant's ID nor its spelling changes. The constant binding and module
root-registration ABI are still design boundaries, not implemented services.

## Lookup and identities

`find(context, text)` returns an existing rooted atom or an empty optional; it never
creates a name. `lookup(context, id)` returns an existing rooted atom or unknown_id.
`name(id)` returns an owned string copy, so a future compaction cannot invalidate the
result. `Term::atom_utf8()` uses this storage; `Term::atom_id()` extracts the immutable
number. The existing private `AtomCell::atom_id` word is the storage identity, and
future tagged atoms must carry that same number rather than a physical table index.

Numeric IDs are qualified by the runtime's storage. The same number in another runtime
can name another atom; a bare `AtomId` is neither a cross-runtime identity nor a GC root.
Keep a rooted Term or a runtime metadata root while an ID must remain live. APIs taking
bare IDs interpret them in the addressed storage; a naked number cannot reveal which
runtime originally issued it. Caller contexts from another runtime fail wrong_owner.

`AtomId` uses the runtime target's unsigned word width, matching the existing term
layout on 32-bit and 64-bit targets. The entry cap and ID space are separate: after
future reclamation there may be at most max_atoms retained entries, while the largest
ever-issued ID continues increasing. Check numeric exhaustion explicitly and report
id_exhausted rather than wrap, reuse IDs or renumber existing atoms. The full 2^26
initial table fits on either target width. `stats()` reports retained count, configured
cap and the highest issued ID; it has no ID before the first successful creation.

## GC and compaction placeholder

`collect()` reserves an atom-table collection API. The first implementation without
atom GC must report not_implemented, never fake successful reclamation. An unsafe
attempt reports unsafe_point. Process-heap GC and atom-table GC are distinct: collecting
one process heap does not prove an atom unused by other processes or runtime services.

Future atom GC must coordinate a runtime-wide safe point and enumerate all atom roots:
process heaps and saved continuations, mailbox/receive/transit data, host Term handles,
loaded/pinned code module/export names and literals, descriptor registries, builtin
atoms and any native metadata retaining numeric atom IDs. An atom cell contains no
process-heap pointer, but its ID is still a reference for the global atom collector.
No reclamation is safe until every such root category and cross-worker handshake is
implemented. The ID/name indexes themselves are storage, not roots keeping all atoms live.

Collection may remove unreachable entries and compact table/string backing storage.
Every surviving entry keeps **exactly the same numeric ID and string bytes**.
Do not renumber survivors, recycle removed numbers, normalize strings or update atom
Terms to physical positions. Removing an atom also removes its name-index entry.
If that spelling is created again after becoming unreachable and being collected,
it is a new entry with a fresh larger ID; there can have been no surviving root to
its earlier identity. A raw saved number for the reclaimed entry then yields unknown_id.

Compaction breaks `physical position == AtomId`, because surviving IDs have gaps.
When GC is implemented, numeric lookup will need a different private strategy, such
as binary search over compacted entries ordered by stable ID. **Do not implement or
declare that second lookup type now.** The public create/find/lookup/name APIs stay
the same; the current proposal uses only checked direct indexing. GC may rebuild the
name index without changing any surviving association. Failure must preserve both
indexes and all live identities; no partially compacted state may be observed.

The future collector's result reports before/after entry counts and released backing
bytes, not reassigned IDs. Reclamation can free room under the retained-entry cap;
it cannot restore exhausted numeric identity space.

## Errors and integration

`AtomStorageError` distinguishes invalid startup options/names, limit_reached,
id_exhausted, unknown_id, wrong_owner, expired_context and out_of_memory, plus the
collection placeholders. Duplicate-name lookup at capacity still succeeds subject
to caller-root allocation. Checked creation/lookup catches allocation failure into
out_of_memory; collection publication must also preserve invariants on failure.

`TermFactory::atom` maps storage limits/ID exhaustion/allocation failure to the
existing TermError::resource_limit, invalid names to invalid_encoding (or invalid_argument
for a length-policy failure), and context failures to the matching term error.
Non-atom `atom_id()`/`atom_utf8()` accesses return wrong_type. No name or numeric value
may be overwritten through this API. Existing atom Terms keep their semantic identity
through process-heap copying and atom-table compaction.

CodeServer may index immutable name spellings for resolution. Any retained numeric atom
references, including its proposed name_atom accessor, must also register runtime
metadata roots before atom GC can be enabled. This sketch supplies no new global-Term
lifetime exception and does not implement that accessor's ownership protocol.

## Review and later validation

Review the retained-entry cap versus numeric-ID exhaustion distinction, zero-based IDs,
runtime-local singleton lifetime, atomic dual-index publication and global root coverage.
The following implementation tests are deferred: duplicate names (including at cap),
sequential successful IDs after injected failures, Unicode/name validation, cap boundaries,
independent runtimes, concurrent creators, root-versus-collection races, retained names
and IDs across compaction, collected-name reinterning and identity exhaustion without
wrap. No alternative lookup algorithm or collector is part of this API-only artifact.

Validation: standalone/combined C++23 syntax with warnings as errors, option/API type
assertions, clang-format, repository clang-tidy, Lizard and local documentation links
pass on macOS arm64. Lizard has no function bodies to measure. No runtime behavior,
full-build gate, other-platform validation or commit is claimed for this review draft.
