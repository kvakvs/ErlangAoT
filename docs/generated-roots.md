# Generated roots and stable allocation

Pattern/guard step 11 uses stable storage without collection. Revision-4 module
registration requires generated root scopes; the descriptor fields and one-word
term encoding are unchanged. Rebuild generated objects and native consumers.
`erlang_aot_register_module_v4` rejects revisions 1–3 before reading dependent
fields. Atom-slot reads retain their revision-3 service and checked failures retain
the revision-2 channel.

Each generated function calls `erlang_aot_roots_enter_v4(context, count)` before
loading its arguments. The runtime allocates a zero-initialized target-word buffer,
bounded by 1,000,000 live words and 4,096 frames per context. Failed registration
sets the exact infrastructure failure and returns null; generated code checks the
channel before touching the buffer. Original arguments occupy persistent slots.
Every evaluated expression and lazy join publishes its result in a slot before
another expression or generated call. Failed candidates clear their temporary
slots before the next candidate; original arguments remain available.

Every return calls `erlang_aot_roots_leave_v4(context, buffer, result)`. Successful
results become owned Terms in the parent's handoff (or outer host handoff) before
the callee buffer is released. The checked failure channel already owns an error
payload before failure cleanup. Null entry buffers need no release. LIFO violations
are infrastructure errors. Native host invocation scopes restore their original
depth even after exceptions, preserving any enclosing caller's frames and first
failure. The host copies the result/failure before scope cleanup.

Heap allocation is the safepoint boundary for future tracing: all live generated
values must already be registered. Root-buffer bookkeeping itself never collects;
caller roots and host argument Terms cover entry until argument slots are written.
Specialized clones retain scopes; dispatchers forward without heap allocation.
These rules do not implement tracing, moving GC, suspension or graph copying.
Host access is serialized. Raw words and allocation spans remain private borrows.

Current admission remains small integers, owned atoms, nil and the canonical empty
tuple. Factories check their weak context lifetime; atoms retain immutable spelling
pins and reject foreign-runtime words. Reservations retain backing for safe rollback
after context teardown, while expired handles deny further access. Concrete compound
Terms, extracted values and compound badmatch payloads must prove retained ownership
in step 12 before admission is enabled; immediate execution is not that proof.

See [process memory](runtime-memory.md) for accounting and rollback and
[step-11 validation](patternmatch-step11-validation.md) for fault/native/width evidence.
