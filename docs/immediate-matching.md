# Immediate matching

Single-clause function heads execute variables, `_`, ordinary `_Name` bindings,
small integers, atoms, `[]`, `""`, `{}`, aliases and repeated names. Normalized
constant arithmetic is admitted when its result fits the target small-integer
range. Nonempty containers, floats/bignums, body matches, guards and ordered
clauses retain their later owners in the pattern/guard plan.

The private flat match plan consumes normalized patterns and clause-local binding
events. Its inputs are original argument slots; aliases share an input. First
definitions retain tentative SSA words, repeated names request exact equality,
and tests carry explicit success/mismatch edges. Extraction is reserved for
checked container operations. A reusable mismatch does not raise an error;
single-clause exhaustion calls the revision-2 `function_clause` service.

Plan indexing, traversal, scalar nodes and terminal construction share a
100,000-work ceiling. Failed construction returns no plan and retains a source
diagnostic. Normalization retains its separate module-wide budget. Grouping and
folded literal source anchors remain available in emitted inspection IR.

`erlang_aot_exact_v1` distinguishes equal, unequal and runtime failure. It checks
both words using real runtime admission, including atom ownership, before shared
`Term::exactly_equal` dispatch. Raw word equality is confined to canonical admitted
immediates; future boxed terms must extend representation dispatch. Failed
ownership/encoding is an infrastructure failure in the context channel, never
a mismatch. Atom literals load module bindings; equality does not intern atoms.

Both target widths use shared checked ABI integer encoding. Native Windows x64
execution and 32/64-bit object/IR checks are recorded separately in
[step-6 validation](patternmatch-step6-validation.md). Unconditional variable
heads keep compact projection/direct-call IR without extra matching blocks.

[Steps 7–8](guard-control-flow.md) now execute immediate guard services and grouped/boolean guards after a successful head. Ordered clauses and body matches remain deferred.
