# Pattern and guard semantic contract (plan step 1)

This is the acceptance and implementation boundary for
[steps 1–20](../.agents/10-patternmatch.md). Step numbers below refer to that plan.
The executable slice supports ordered clauses and body sequences with exact integer,
atom, tuple/list/string patterns, checked construction/access, aliases, repeated
variables, binding reads and direct calls. [Step 6](immediate-matching.md)
implements immediate head matching; [step 7](immediate-guards.md) adds immediate predicates/comparisons/queries; [step 8](guard-control-flow.md) adds grouped guards and strict/lazy operators. [Step 9](ordered-clauses.md) adds ordered function clauses; [Step 12](container-matching.md) adds tuples/lists/strings; [Step 13](integer-matching.md) adds exact arbitrary integers and checked integer arithmetic; floats/maps/bitstrings/records remain pending.
[Step 3](runtime-atoms.md) adds owned atoms.
[Step 4](scoped-bindings.md) adds clause-local binding analysis and conservative
facts, including located unbound/unsafe/wildcard errors.
[Step 5](pattern-semantics.md) adds bounded pattern normalization, embedded key/size
legality and sibling scope checks independently of runtime availability.

Evidence is pinned to official `maint-29` revision
`21776803ecd11f5fa948732c0ec66b8f325dedfc`, re-fetched and unchanged on
2026-10-02. The separate installed oracle is OTP **29.1.1**, ERTS **17.1**.
[otp.tsv](../tests/fixtures/patternmatch/otp.tsv) records SHA-256 identities for
the reference manual, grammar, lint rules, BIF catalog, suites and actual headers.
Hashes use source bytes with CRLF normalized to LF; no other whitespace or source
changes are permitted. This accommodates the existing Windows checkout without
rewriting it. The checkout is clean; the user removed the old untracked `1.ir` during step 4.

The principal evidence is `expressions.md` (match/compound operators, maps,
bit syntax and guard sections), `erl_lint.erl:pattern/4`, `pattern_map/4`,
`pattern_bin/4`, `guard_test2/3`, `gexpr/3`, and `erl_internal.erl`'s exact signature
tables. The reference manual's summary tables omit some source-defined entries;
the catalog includes `is_integer/3`, `ceil/1`, `floor/1`, `binary_part/2,3` and
native-record `is_record/1`. Preprocessor condition evaluation is not authority
for source guard legality.

## Contexts and rejection policy

Every row names an implementation step or an explicit backlog dependency owner.
**Capability** means legal Erlang requiring an unavailable feature, with an
explicit compiler capability diagnostic until its owner implements it.
**Semantic** means invalid Erlang, diagnosed regardless of reachability.
These are the target distinctions for steps 4/5/7; step 1 does not retrofit
diagnostics into the current compiler. **Mismatch** means runtime selection
failure, not a compiler error. Resource/ownership/internal failures must stay
distinct from ordinary mismatch or a reached guard argument error (steps 2/11).

| Context | Binding and execution rule | Owner | Rejection expectation |
| --- | --- | --- | --- |
| Function clause heads plus their guards | Try heads in source order; candidate bindings feed only its guard/body; failed candidates discard them | 4–9 | Capability until implemented; exhaustion becomes `error:function_clause` (2/6/9) |
| Body match expressions and sequences | Evaluate RHS once, right-to-left chained matches; commit bindings on success; match returns RHS | 4/5/6/10 | Implemented for immediates by step 10; mismatch raises `error:{badmatch,RHS}`. Heap payload roots extend this in 11–12 |
| `case`/`if`/`maybe`, comprehensions | Additional pattern/guard contexts are outside this plan | F16 | Capability; no execution claim from parsing |
| `catch`/`try`, catch patterns/guards | Structured exception handling beyond propagation is outside this plan | F20 | Capability |
| Anonymous/named fun clauses | Function values and captured environments are outside this plan | F18 | Capability |
| `receive` patterns/guards | Mailbox scanning and process execution are outside this plan | F22/F25 | Capability |

## Pattern forms (both admitted contexts)

| Form | Required behavior or legality constraint | Owner | Rejection expectation |
| --- | --- | --- | --- |
| `_` | Never creates a readable binding; occurrences independent | 4/6 | Reading `_` is semantic error |
| `Name`, `_Name`, repeated names | Clause-local single assignment; `_Name` is ordinary; repetitions impose exact equality, including integer versus float distinction | 4/6/12–16 | Unbound/unsafe read is semantic; unequal repetition is mismatch |
| Parentheses, `P1 = P2` compound patterns | Both operands constrain the same value; neither supplies new key/size bindings to its sibling | 5/6, extended 12–17 | Illegal sibling dependency is semantic; valid but incompatible aliases mismatch |
| Atoms, booleans | Spelling-based runtime identity; no compiler-assigned atom IDs | 3/6 | Implemented in heads; different literal mismatches |
| Integer/character literals, unary signs and constant arithmetic | Accept only legal, evaluable constant pattern expressions; chars are integers | 5/6/13 | Invalid/nonconstant expressions semantic; different value mismatches |
| Floats, arbitrary integers | Exact pattern equality, target-width-independent values; signed-zero details follow OTP | 13/14 | Capability until representation exists; different value mismatches |
| Empty list/tuple | Canonical admitted immediates | 6 | Implemented in heads; wrong shape mismatches |
| Tuples, lists, improper tails, strings | Exact tuple arity; cons/nil shape; strings are lists; nested patterns | 11/12 | Implemented construction/access/equality in heads and body matches; wrong shape mismatches |
| String/list-literal `++` pattern prefix | Lint permits a literal string or integer/character cons prefix, including empty prefix; arbitrary variable prefix is illegal | 5/12 | Nonliteral/invalid prefix semantic; nonmatching prefix mismatch |
| Maps `#{Key := Pattern}` | Partial matching, including empty-map type check; keys are legal guard expressions over incoming bindings; exact key identity | 5/15 | `=>`, unbound keys or illegal key calls semantic; missing key/wrong type mismatch |
| Bitstrings and binaries | Segment type/size/unit validation, sequential earlier-segment size bindings, incoming scope for aliases, final unsized tails, UTF segments | 5/16 | Invalid specifier/size scope semantic; insufficient bits/value mismatch; capability until step 16 |
| Expanded tuple records, fields, wildcard fields, record indices | Resolve declared record/field names and expand to tuple constraints; preserve locations | 5/17 | Missing declarations/bad fields semantic; wrong tag/shape mismatch |
| OTP native/anonymous records | Distinct from expanded tuple records; not covered by step 17's tuple representation | F17 (native representation) | Legal forms capability; invalid declarations/fields semantic |
| Calls, variable arithmetic, updates and other expressions used as patterns | Permissive expression-side AST must still be checked as a pattern | 5 | Semantic error (or syntax error for grammar-restricted head forms) |

The map and binary alias cases are not ordinary left-to-right assignment.
For example, `#{K := V} = #{key := K}` and `<<X:N>> = <<N:8>>` cannot supply
their own sibling key/size variables. A prior body binding can supply a map key;
an earlier binary segment can supply a later segment size. Seeds check both sides
of that distinction against OTP.

## Guard forms, resolution and evaluation

The companion [guards.tsv](../tests/fixtures/patternmatch/guards.tsv) is the
signature-level matrix: every row has category, exact name, arity, owner and
rejection policy. Its entries are checked for exact set equality with the pinned
`guard_bif`, `new_type_test`, `old_type_test`, `arith_op`, `bool_op`, and `comp_op`
clauses. Adding or removing an upstream signature cannot silently pass the test.
The following rows cover syntax/control-flow rules that are not BIF signatures.

| Form | Required rule | Owner | Rejection expectation |
| --- | --- | --- | --- |
| Variables/literals | Only established bindings readable; final success requires atom `true` | 3/4/7/8 | Unbound reads semantic; any other final value rejects guard |
| Comma and semicolon | Comma is conjunction; semicolon starts a fresh alternative after false or a reached argument error | 8/9 | Implemented by step 8; all alternatives fail means clause mismatch |
| `andalso/2`, `orelse/2` syntax | Lazy RHS, term-valued intermediate results; reached error fails the enclosing guard, not a recoverable false operand | 8 | Semantic operand error rejects reached guard; invalid syntax/calls still diagnosed in skipped branches |
| Strict `not/1`, `and/2`, `or/2`, `xor/2` | Boolean operands, eager evaluation; no substitution of lazy semantics | 8 | Wrong types reject guard |
| Equality/order operators `==`, `/=`, `=:=`, `=/=`, `<`, `=<`, `>`, `>=` (all /2) | Exact versus numeric equality; structural order over the admitted domain | 7/12–16/18 | Capability until admitted representation supported; no raw boxed-word equality |
| Unary `+`, `-`, `bnot`; binary `+`, `-`, `*`, `/`, `div`, `rem`, `band`, `bor`, `bxor`, `bsl`, `bsr` | Exact arities in catalog; numeric semantics and overflow/promotion follow OTP | 13/14/18 | Capability until services exist; bad arguments reject reached guard |
| Auto-imported BIF calls | Exact signature plus local/import/no_auto_import resolution determines legality | 7/18 | Wrong arity, shadowed/imported ordinary function or suppressed auto-import semantic |
| `erlang:Bif(...)` and `erlang:'Op'(...)` | Explicit qualification bypasses auto-import shadowing; only guard BIFs or admitted guard operators legal | 7/8/18 | Unknown/wrong signature semantic; qualification does not legalize arbitrary functions |
| Legacy top-level tests | `integer/1`, `float/1`, `number/1`, `atom/1`, `list/1`, `tuple/1`, `pid/1`, `reference/1`, `port/1`, `binary/1`, `record/2`, `function/1` | 7/8/17/18 | Legacy-only names nested or qualified are semantic errors; clashes checked |
| `float/1` ambiguity | Top-level unqualified legacy test means `is_float`; nested or explicit `erlang:float/1` is legal numeric conversion, not the legacy predicate | 7/8/14/18 | Legacy predicate implemented by step 7; conversion remains deferred; bad conversion rejects guard |
| Tuple/list/map/binary/record construction; map update | Legal guard expressions when all children are legal; checked allocation/access; map update uses incoming bindings | 11/12/15/16/17/18 | Tuple/list construction implemented; later map/binary/record families await their steps; reached semantic failures reject guard |
| Record field/index expressions; `is_record/2,3` | Declaration/field validation; lint restrictions on literal tag/arity, including OTP native-record distinctions | 7/17/18; native forms F17 | Bad declarations/argument forms semantic; wrong value shape rejects guard |
| `is_integer/3` | OTP 29 inclusive range predicate; exact signature is legal | 7/13/18 | Capability until service exists; OTP cases include both endpoints and wrong type |
| Identity/function predicates | `is_pid/1`, `is_port/1`, `is_reference/1`, `is_function/1,2` can classify admitted terms; positive identity/fun values require their owners | 7/18; F07/F18 | No forged identity admission; capability for missing representations, invalid function arity argument follows OTP |
| `self/0`, `node/0,1` | Legal signatures but process/distribution services are outside this plan | F07/F22/F26 | Explicit unavailable-service capability diagnostic |
| `is_record/1` | Source catalog includes native-record classification; expanded tuples are not native records | F17 (native representation) | Explicit capability diagnostic until owner provides representation/service |
| Assignment, arbitrary local/remote/dynamic calls, list `++/2`, `--/2`, send `!/2`, funs/comprehensions/control flow in guards | Not in the guard expression grammar/semantic allowlist | 7 | Semantic error, including unreachable operands; builtin registration cannot authorize it |

Step 18 reconciles every in-scope signature over the complete admitted value domain;
step 19 checks specialization without trusting specs; step 20 publishes the
scoped contract. Passing the immediate-only baseline does not discharge any of
these later obligations or prove heap lifetime, native records, scheduling or GC.

## Reproducible evidence

`patternmatch_evidence` is a CTest workflow enabled with compiler, runtime and
`BUILD_TESTING=ON`. It requires the pinned checkout and a working installed OTP;
missing dependencies fail, rather than count as passed coverage. It:

1. Verifies revision, tracked-source cleanliness, manifests and the guard catalog,
   then deliberately changes a manifest hash in memory and requires rejection
   before parsing or extracting any source.
2. Preprocesses/parses original `guard_SUITE.erl`, `match_SUITE.erl` and
   `trycatch_SUITE.erl` through the public CLI. It maps real compiler, stdlib,
   kernel, common_test and syntax_tools include locations, enables `maybe_expr`
   and disables `compr_assign`. This is syntax coverage only.
3. Runs authored `.erl` seeds through OTP `epp:parse_file` (including embedded
   error forms), then `compile:forms` separately. `acceptance.term` states syntax,
   semantic and diagnostic expectations; `cases.term` states results and stable
   error class/reason. Clause selection is observable through distinct results.
   Negative cases include wrong arity, unreachable calls, assignment, legacy
   nesting, wildcard reads, invalid patterns, sibling scopes and BIF shadowing.
4. Extracts the entire unchanged `bif_SUITE:first/2` and `guard_SUITE:id/1` clauses
   after source hash verification. It retains upstream license notices and records
   source path/function, source/clause/generated hashes, declarations and all
   wrapper changes. New `answer`/`client` module/export declarations fit the
   existing consumer; the authored `answer:identity/1` forwards to `client:id/1`.
   No helper body is weakened. OTP checks integers, empty list and empty tuple.
5. Emits both modules through the public CLI at O0/O2, each with default/disabled
   specialization policy, and executes them twice through the existing separate native
   runtime consumer. It retains that consumer's immediate endpoints, cross-module
   call, ABI rejection, missing-runtime link failure, deferred-allocation recovery
   and teardown checks. This is the executable baseline, not execution of suites
   or the new pattern/guard seeds.

O0 bypasses specialization internally; both CLI flag policies are exercised.
The unchanged projection helpers also remain generic at O2 because they offer no
removable checks. This baseline does not claim source-driven specialized matching.

Build-local `provenance.json`, `suites.json`, `helpers.json` and `oracle.txt` record
the actual run. [The step-1 validation record](patternmatch-step1-validation.md)
preserves the exact versions and measured outcomes. Authored fixture and catalog
hashes in [fixtures.tsv](../tests/fixtures/patternmatch/fixtures.tsv) are reviewed
expectations; tests never regenerate them. Existing corpus/grammar tests remain
part of the full gate. Later steps extend executable coverage while retaining this
distinction between legal source, accepted syntax and emitted behavior.
