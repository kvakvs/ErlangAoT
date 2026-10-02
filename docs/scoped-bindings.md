# Scoped bindings and conservative facts

Pattern/guard plan step 4 adds semantic binding identities without enabling
pattern, guard, clause-dispatch or body-sequence execution.

A binding identity is a function-relative pair of zero-based clause and local
definition indices. Each definition owns its decoded name, original AST expression
and optional original argument position. Each occurrence records a definition,
read or exact-equality check, together with its head/guard/body context. Re-running
analysis on the same syntax produces the same identities.

Each clause starts with an independent incoming environment. A pattern candidate
borrows that environment and accumulates tentative definitions separately. `_`
does not define a name; `_Name` does. Reusing a name records exact equality against
the original identity. Discarding or invalidating a candidate does not publish its
names. The guard reads the completed tentative head. The body is analyzed on the
successful candidate path. These are semantic environments, not executable
selection: runtime guard rejection and failed-candidate rollback await step 9.

Body sequences publish successful match bindings to following expressions.
A match visits its RHS before its LHS pattern, including right-associated chains.
A parenthesized compound pattern is visited as a pattern, not as a sequence of
assignments. Already-bound names remain equality constraints.

Ordinary expression siblings share incoming readable names. Definitions are
exported after the whole expression, and sibling definitions of the same name
share an equality obligation. Thus `{X = 1, X}` diagnoses an unbound read, whereas
`{X = 4, X = 3}` has legal bindings and requires a runtime equality check.
Explicit `begin` sequences publish bindings within their own sibling.
Definitions on the optional RHS of `andalso`/`orelse` are readable within that RHS
but unsafe afterwards. Guard matches are diagnosed even in unreachable operands;
no guard expression publishes bindings.
Unsafe status takes precedence when sibling environments merge, even if another
sibling defines the same name unconditionally.

Pattern traversal recognizes binding-bearing tuple/list/record/map/binary syntax.
Map keys and segment sizes are read contexts. Step 5 adds
[bounded pattern normalization](pattern-semantics.md) and enforces incoming-only
key reads and binary-local preceding-segment size reads, including compound
sibling isolation. Branch, exception, comprehension and closure
scopes are opaque and retain their capability gates; this is not their semantic
implementation.

All walks are iterative. The module-wide default budget is 1,000,000 work units,
charging visited nodes/tasks and copied environment entries. Exhaustion produces
one located error and clears the module's binding/normalization tables. Step 5
also clears those tables after ordinary semantic errors. Ordinary unbound, unsafe
and wildcard reads use the original AST anchor, including macro/include origins.

Only whole original arguments retain projection provenance through grouping or
aliases. Extracted values and new body definitions have unknown facts. Inference
resolves reads through identities and never treats definitions or equality checks
as parameter reads. Specifications cannot narrow these facts. Lowering requires
an available candidate SSA value and rejects unavailable bindings. Public
`--print-types` expression lines include `binding=clause[N].local[M]` for reads.

Step 6 executes repeated parameters, aliases and immediate patterns through
[checked matching](immediate-matching.md). Steps 7–8 admit immediate services and grouped/boolean guards. Multiple clauses, body matches,
sequences and later containers remain capability-gated. Identity/projection,
atoms and direct-call execution keep their existing behavior.

Evidence comes from pinned `maint-29` expressions documentation, `erl_lint.erl`
(`exprs/3`, `expr_list/3`, `vtmerge_pat/3`, variable/guard checks), and complete
`match_SUITE` helpers. See [step-4 validation](patternmatch-step4-validation.md).
