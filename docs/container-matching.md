# Tuples, lists and strings

Function heads and body matches share checked tuple/cons extraction and bounded
structural exact equality. Source expressions construct nested tuples, proper or
improper lists, and Unicode strings represented as lists of integer codepoints.
The existing guard/body services implement `hd`, `tl`, `length`, `tuple_size`,
`size` for tuples, and one-based `element`, plus predicates and comparisons.

Tuple backing contains a target-word arity header followed by fields. Cons backing
contains two words, head and tail. Empty tuple and nil retain canonical immediate
encodings. A process-owned index records exact published tagged starts, semantic
kind and extent. Contextual admission proves membership before any object load;
forged, interior and foreign addresses never reach a header dereference. Compiler
IR uses runtime inspection services and contains no representation-dependent heap
loads.

Factories validate every child's ownership before reserving backing. Tuple fields
and complete list spines initialize before index publication and commit. Failure
erases only the new index entries, rolls back backing/counters and preserves earlier
values. Constructor marshalling/metadata allocation errors retain exact checked
statuses. Each constructor accepts at most one million elements, subject to the
process backing ceiling. There is no collector or implicit cross-heap copying.

Host `Term` handles pin stable backing and immutable object metadata. Extracted
children inherit that ownership; raw cells contain words, avoiding shared-pointer
cycles. Teardown invalidates the context before releasing its ownership. Retained
handles remain safe to destroy, but compound access after expiration returns
`expired_context`. Same-heap copies and argument/return handoffs preserve identity;
another process cannot admit or silently copy the graph.

Flat match plans allocate separate SSA candidate slots for fields, heads and tails.
Tuple arity and cons shape checks dominate extraction. Short/wrong-shaped candidates
take the caller's mismatch edge; infrastructure failures stop selection. Literal
string prefixes replace their terminal nil constraint with the suffix pattern,
including nested literal-list tails. Aliases constrain the same original candidate;
repeated variables use structural equality without reconstructing containers.
All indexing/scheduling shares the existing 100,000-work plan ceiling.

Generated arguments, constructor scratch arrays and every inspected result live
in bounded revision-4 runtime root buffers. Rejected candidates clear temporary
slots. Successful bodies keep bindings available across allocating nested calls;
result ownership transfers before frame pop, and badmatch retains the saved RHS
before cleanup. This establishes stable-backing lifetime behavior, not moving GC.

Equality and order use an iterative worklist bounded to one million processed and
pending pairs. Tuple arity precedes field order; lists compare heads before arbitrary
tails. Independently allocated equal graphs compare by value. Resource or allocation
failure cannot be mistaken for inequality, false or ordinary guard rejection.
The admitted numeric domain remains small integers until the next implementation
steps; maps, bitstrings and records have their own later services.

See [step-12 validation](patternmatch-step12-validation.md),
[generated roots](generated-roots.md) and [process memory](runtime-memory.md).
