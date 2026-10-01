# Pattern semantic analysis

Pattern/guard step 5 validates function-head patterns and body-match left operands
before lowering. Both `RestrictedPattern` and `PatternCandidate` use the same
private validator; permissive expression syntax cannot authorize calls, control
expressions or assignments inside a pattern. Other source contexts retain their
existing capability gates.

`Function::patterns` owns a flat normalization table. Each entry keeps its original
expression/source anchor, ungrouped expression identity, kind, ordered AST child
references and optional owned scalar constant. Parentheses are transparent;
characters normalize to integers. Arithmetic constants preserve arbitrary integer
precision and binary64 type, including signed zero. Only OTP pattern arithmetic
operators are folded. Nonconstant arithmetic, boolean/comparison expressions and
evaluation failures such as `1 div 0` are semantic errors. Strings and proper
literal integer/character list prefixes are recognized, including explicit list
tails; arbitrary `++` operands are invalid.

Expression `=` evaluates its RHS before analyzing its LHS pattern. Inside a
pattern, `=` creates an alias/compound constraint, with both operands matching
the same future value. It does not sequence definitions. Variables, wildcards,
tuple/list/string, map, bitstring and record syntax remain distinguishable for
later match planning. Container metadata stays in the immutable AST. Record
expansion, field/layout validation and native-record behavior remain assigned to
step 17; no record execution is introduced here.

Every compound operand, tuple/list element, map field and head argument reads
the incoming environment. Definitions accumulate separately for equality checks
and eventual publication. A map key cannot read a variable freshly bound by a
sibling value, argument or alias operand. An individual binary owns another
readable scope: its segment size sees incoming names and that binary's preceding
variable segments, but not its own segment's new variable or a sibling binary's
bindings. Thus `<<N:8,X:N>>` is legal, while `{N,<<X:N>>}` and
`<<N:8>> = <<X:N>>` need an already-bound incoming `N`.

Map fields require `:=`. Keys and sizes accept read-only guard expressions,
including nested containers and qualified `erlang` operators/BIFs. Their legality
catalog is checked against pinned `erl_internal:guard_bif/2` and
`new_type_test/2`, independently of the preprocessor evaluator and runtime
capabilities. Local/imported shadowing and global/selective `no_auto_import`
metadata are respected; explicit `erlang` qualification/imports resolve to BIFs.
Ordinary guard validation and execution remain step 7. Every embedded expression
is checked, including skipped boolean operands. Constant bad arithmetic in a key
or size is legal syntax that can fail matching later, unlike arithmetic in a
literal pattern.

Binary checks reject nested-container/alias segment patterns, invalid/conflicting
modifiers, incompatible alias units, invalid unit ranges, unit-without-size for
integer/float defaults, UTF size/unit combinations, typed/sized literal strings
and nonfinal unsized binary segments. Nested binary construction in keys/sizes
checks modifier rules without applying binary-pattern-only restrictions. Dynamic,
negative or noninteger size expressions remain legal where OTP accepts them;
their future match-failure behavior is not executed by this step.

All walks use explicit task/value stacks. The shared module binding/pattern budget
defaults to **1,000,000 work units** and charges nodes, tasks, scope copies,
metadata, retained literal text, child edges and arithmetic scalar storage.
Constant evaluation additionally caps each intermediate at **10,000 decimal
characters** and shifts at **1,000,000 bits**. These are resource limits, not
Erlang semantic failures. The parser independently defaults to 256 grammar nesting
levels, with a hard 512 ceiling. Synthetic private tests cover 12,000 pattern
levels independently of that parser admission limit.

Any semantic error or exhausted work budget clears every function's binding,
clause-definition and normalization tables. The driver retains its existing
whole-batch artifact transaction. Diagnostics use original expression anchors,
including include/macro provenance. Existing capability diagnostics still reject
legal patterns requiring unavailable matching or runtime operations; semantic
errors additionally identify invalid pattern syntax and binding dependencies.
No executable pattern, guard or clause-dispatch feature is enabled here.

See [step-5 validation](patternmatch-step5-validation.md) for provenance and tests.
