# Immutable maps and computed-key matching

Maps use owned immutable tables sorted by exact term key order. Construction
evaluates entries in source order and keeps the last association for duplicate
keys. Integer and float keys remain distinct, including recursively inside
compound keys; positive and negative float zero are distinct keys. Updates stage
a new table and publish once. Exact updates require a key present after preceding
updates in that expression. Failed construction leaves the source map and heap
publication accounting unchanged.

Map comparison first compares sizes, then exact ordered keys, then values using
the enclosing exact or numeric comparison. Insertion history and allocation
identity do not affect equality. Construction, update, lookup and comparison
charge bounded work; infrastructure failures propagate separately from semantic
errors. Stable process backing owns keys and values across later allocations.
Foreign and expired heap terms cannot enter or access a map.

Function heads and body patterns test map shape, then perform each required key
lookup through the checked runtime service. Extra keys are allowed. An empty map
pattern tests type only. Duplicate and equal computed keys retain every value
constraint. Key expressions use the incoming binding scope already established
by semantic analysis; sibling pattern definitions remain unavailable. Key
computation and missing-key failures reject a candidate or raise the body's
`{badmatch,RHS}`. Allocation, ownership and resource failures stop execution.
Extracted values and temporary keys use the existing generated root scopes.

Expressions and guards support construction, association/exact update,
`is_map/1`, `map_size/1`, `map_get/2`, and `is_map_key/2`. Body query/update errors
retain `{badmap,Map}` or `{badkey,Key}` payloads; guard errors reject the reached
alternative. Operand evaluation precedes map validation, preserving the first
reached operand error. Map-key atoms participate in module registration even
when they occur nowhere else in a function.

The project-owned differential harness extracts complete size/get helpers from
the pinned OTP `map_SUITE` and labels update/key/duplicate-test adaptations.
It runs the generated Erlang modules on installed OTP and compares native
results in four policies. Separate host tests cover ownership, rollback, work
limits, malformed service input and retry. Generated-call fault injection checks
root cleanup and infrastructure propagation. Cross-target object checks are
separate from native execution; GC and cross-process copying remain deferred.
