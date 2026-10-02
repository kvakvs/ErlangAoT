# Runtime atoms and booleans

Pattern/guard step 3 implements atom literal expressions, including `true` and
`false`, through runtime-owned module bindings. Patterns, guards, equality and atom
ordering remain with their later steps. Booleans are ordinary atoms, not integer
or LLVM `i1` values.

`RuntimeOptions::max_atoms` accepts 1 through 2^26 retained entries, default 2^20.
Each runtime owns one `AtomStorage`, shared by its contexts. Interning is lazy and
deduplicates exact UTF-8 spelling. Module/export metadata also consumes this limit.
Existing spellings succeed at capacity. There is no atom collection or eviction.
Calls require host serialization within each runtime, as do other runtime services.

Spellings contain at most 255 Unicode scalar values. Empty strings, embedded NUL
and supplementary characters are accepted; malformed, overlong, surrogate and
out-of-range UTF-8 encodings are rejected. No normalization or case folding occurs.
The input bytes are copied, and both indexes publish together or roll back.

## Ownership and representation

The generated representation remains one target-sized word, with low six bits
`0x0b` and a non-recycled payload. An atomic process-wide counter reserves payloads;
tables, spelling records and limits remain runtime-owned. This replaces the earlier
dense runtime-local numbering sketch: disjoint raw words are necessary to reject a
foreign atom word even when two runtimes intern the same names in the same order.
Failed allocations may consume counter values. Exhaustion fails instead of wrapping;
32-bit processes have 2^26 lifetime word identities, shared across runtime instances.
IDs are internal and must never be serialized or used to order atoms.

Host `Term` is now a word plus an optional shared immutable atom record. It is not
the generated ABI layout or a heap cell. Copies and structured error payloads pin
the spelling after context, module and runtime teardown. Generated arrays and
reserved heap cells still store `Word`, with no dependency on `sizeof(Term)`.

`Term::from_word(word)` still admits only owner-independent integers and empty
containers. `Term::from_word(word, context)` additionally checks atom membership in
that context's runtime. Invocation arguments/results, builtin marshaling, heap
copies, native error payloads and badmatch materialization all use this owner check. Cross-runtime atom
copying is rejected with `wrong_owner`; explicitly copy `atom_utf8()` and intern it
in the destination to remap. `TermFactory::atom/boolean` uses the live context's
table and rejects moved-from or expired factory bindings.

## Module initialization and failure

ABI revision 3 extends descriptors with `(UTF-8 pointer, size)` entries and a
target-sized count. Current registration uses `erlang_aot_register_module_v4`;
revision-1/2/3 descriptors reject before reading dependent fields. Term tags and the
revision-2 checked failure channel remain unchanged.

The compiler assigns deterministic spelling slots, never atom IDs. Registration
validates spellings, interns metadata and literals, and builds an immutable slot
vector privately. CodeServer publishes the registry, slots and code-image ownership
together. Allocation/capacity failure leaves no published module or partial slots.
Validated atoms already interned before a later failure remain in the bounded table;
retry can reuse them. Invalid spelling validation precedes any interning.

Each runtime has separate bindings even for the same linked image. Expression
evaluation calls `erlang_aot_atom_v3` to read a slot through the context's CodeServer;
it never interns a spelling or patches an image-global atom word. Failed slot lookup
enters the checked failure channel before the result can be used. A remote module
must be registered before executing its atom expressions; registration then permits
a clean retry. Generated call handles reject contexts belonging to another loaded
module instance.

The immutable descriptor address is the binding key. Its image must remain pinned
through module lifetime; reusing an already published key for another module is
rejected. Registration copies spelling bytes, and use-site lookup
never dereferences descriptor bytes. LoadedModule and resolved handles retain slots
and code image together. Dynamic unloading and concurrent publication stay deferred.

## Evidence

`codegen_atoms` compares literal leaves adapted from pinned `guard_SUITE:rb/3` and
`csemi2/2` with OTP. Adaptations preserve the atom-return behavior under test and do
not claim execution of the original guards or clause selection. It retains license,
revision, source/function hashes, declarations and generated-source provenance.
Authored cases add Unicode, NUL, empty atoms, a misleading spec, local/remote calls,
both CLI modes, registration failure/retry, independent runtimes and retained error
payloads. Four O0/O2 and specialization policies run the separate runtime consumer.
Foreign object tests cover target-width descriptor/atom-service emission; they do
not claim execution on foreign hosts.
