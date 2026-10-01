# F13 Pattern matching and F14 Guards — implementation plan

Created 2026-10-01 from [the feature backlog](01-todo.md). Necessary prerequisites
are included below in implementation order. This is a plan, not an implementation
record; all steps start incomplete.

## Scope

Implement matching and guards in function clauses and body match expressions,
including atoms, numbers, tuples, lists/strings, maps, bitstrings and expanded
records. Include only the runtime construction, checked access, comparison,
ownership and failure services these forms need. Ordered function clauses and
body sequences/matches are the required F15/F16 slices.

The compiler currently executes one clause with distinct variable/wildcard
parameters and one small-integer, parameter-read or direct-call body expression.
The parser retains broader syntax, but atoms, heap construction, general equality,
guard BIFs and Erlang failure propagation are not executable. The steps below
replace these gaps in one sequence; there is no separate prerequisite plan.

Other source contexts and runtime features remain in the backlog: case/if,
catch/try, funs, receive, process/port/reference services, recursion, scheduling,
message copying, garbage collection and production executable linking. They are
not completion requirements here. Guard services requiring those features remain
explicitly unavailable; predicates may still classify the admitted value domain.
Do not claim complete Erlang guard coverage or all F13/F14 source contexts.

## Validation rules

The official maint-29 head was checked on 2026-10-01 and matched the checkout and
pin `21776803ecd11f5fa948732c0ec66b8f325dedfc`. Preserve the checkout's untracked
`lib/stdlib/src/1.ir`. At implementation start, recheck upstream and follow
[otp-reference.md](../docs/otp-reference.md); synchronize pin, checkout, corpus hashes
and grammar evidence while preserving historical records. Record the installed OTP
oracle version separately. This consolidation adds no new implementation evidence.

Use pinned `system/doc/reference_manual/expressions.md`,
`lib/stdlib/src/{erl_lint,erl_internal}.erl` and compiler suites as evidence.
All suite names below refer to `references/otp/lib/compiler/test/`. Distinguish:

- Preprocess/parse original suite files with real includes and feature flags;
  parsing a suite is not executable coverage.
- Compile selected complete helpers or explicitly labeled adaptations with OTP
  and the public compiler CLI. Record source path, function/arity, revision, hash,
  declarations and adaptations; preserve license notices. Do not remove the
  behavior under test to fit the supported subset.
- Emit objects and execute them in the existing separately linked runtime consumer.
  Compare values, selected clauses and error class/reason, not raw IDs or unstable
  stack formatting. Unrelated Common Test harness code need not compile natively.

Every step must pass its success criteria and tests, including negative cases.
Run native workflows at O0/O2 with specialization on/off where applicable. Preserve
misleading-spec cases, positional/project modes, failed-batch nonpublication and
post-failure recovery. Keep focused invariant/fault tests only where real source
cannot practically establish ownership, budgets or injected failure behavior.
Unavailable tools and deferred cases cannot count as passing evidence.

Preserve internal C++23 APIs, owned ASTs, source provenance, bounded traversal,
target-derived layouts and LLVM-free runtime code. Document field/function intent
in one or two lines and use clang-format. Before each clean implementation commit,
freshly configure build/debug with compiler, runtime and BUILD_TESTING=ON; build,
run CTest, then `cmake --build build/debug --target check-quality`. Pass Lizard and
clang-tidy without relaxed thresholds or suppressions. Record native platform
results separately from foreign-object/32-bit layout checks and list missing runners.

## Ordered implementation steps

### 1. Fix the semantic matrix and OTP evidence

- [ ] Refresh/check the reference using the documented procedure, preserving local
  work. Review pattern and guard rules against the pinned lint/compiler sources.
- [ ] Create a matrix of pattern forms, guard operators/BIF name-and-arity pairs,
  the two admitted source contexts, legal-but-deferred features and invalid constructs.
  Include legacy guard tests, qualified BIFs and OTP 29 additions such as
  `is_integer/3`; do not infer legality from the preprocessor evaluator's subset.
- [ ] Seed small real `.erl` fixtures and an OTP oracle for acceptance, results,
  selected clause and error class/reason. Cover `_` versus `_Name`, repeated names,
  compound patterns, guard alternatives and invalid calls.

**Success criteria:** Every matrix row has an implementation step or an
explicit dependency owner and rejection expectation. Oracle fixtures record exact
source/oracle versions and distinguish syntax acceptance from semantic acceptance.
The initial slice and later completion boundary are reviewable without guessing.

**Tests:** Parse `guard_SUITE.erl`, `match_SUITE.erl` and `trycatch_SUITE.erl` with real
includes. Compile unchanged `bif_SUITE:first/2` and `guard_SUITE:id/1` on admitted
immediates as a baseline. Record provenance and reject a deliberately stale hash.

### 2. Implement generated-call failure propagation (F20/F02 slice)

- [ ] Coordinate a minimal F20/F02 contract for successful results, clause mismatch,
  guard rejection, Erlang errors and runtime infrastructure failures. Decide the
  concrete transport before emitting fallible code: explicit status/result or a
  checked context error channel; document why the chosen scheme fits later F20.
- [ ] Implement propagation through local/remote generated calls, runtime services,
  registration and native consumers. Version descriptors/signatures if their
  contract changes; retain target-derived layout and native C++ service linkage.
- [ ] Preserve enough structured error information for `function_clause` and
  `badmatch` with its offending value, with rooted ownership where needed. Add
  heap payload ownership in step 11. Full catch/try is separate.

**Success criteria:** A failed nested generated call cannot become a valid
term or continue its caller's body. Wrong ABI consumers are rejected. Tests observe
success and structured failure separately, no C++ exception escapes the boundary,
and a later independent invocation has no stale failure state. Ordinary mismatch
and guard rejection are silent selection outcomes, not feature diagnostics.

**Tests:** Execute baseline helpers across two generated modules. Use the existing
native service failure seam to verify propagation, cleanup and successful retry.
Steps 6/10 add source function_clause/badmatch; step 13 adds real badarith.

### 3. Implement atoms and boolean values (F06)

- [ ] Implement runtime-owned stable atom storage, spelling validation, configured
  limits, deduplication and transactional module spelling/slot initialization.
- [ ] Lower atom literals and `true`/`false` via module bindings; never bake in
  compiler-assigned IDs or intern on each expression evaluation.
- [ ] Admit owned atoms through host Terms and error materialization; define
  cross-runtime rejection/remapping and preserve module lifetime rules.

**Success criteria:** Equal spellings in separate modules share identity within
one runtime. Foreign atom words cannot be mistaken for local atoms. Registration
failure leaves no published partial module and has a documented atom-table policy.

**Tests:** Compile atom-return leaves adapted from `guard_SUITE.erl` and call them across
modules. Compare spellings and booleans with OTP; test Unicode, deduplication,
capacity failure and independent runtimes without comparing raw atom IDs.

### 4. Introduce scoped bindings and conservative value facts

- [ ] Extend `semantic/declarations.hpp` and `bindings.*` with stable clause-local
  binding identities and explicit reads, definitions and already-bound checks.
  Preserve original argument provenance where applicable.
- [ ] Represent incoming bindings and tentative candidate bindings separately.
  `_` creates no binding; `_Name` behaves as a normal name; repeated variables
  request exact equality. A successful pattern makes its bindings available to
  its guard; only a successful candidate makes them available to its body.
- [ ] Define body-match scopes; guards may read bindings but cannot assign.
  Update inference, lowering and inspection consumers with conservative facts for
  new identities now; step 19 checks optimization across the complete value domain.

**Success criteria:** CLI diagnostics identify unbound/unsafe reads and `_`
reads with original locations. Existing identity/projection functions still work.
Two clauses using identical variable names have independent identities, and failed
candidates leave their incoming environment unchanged.

**Tests:** Compare binding legality from selected `match_SUITE.erl` cases with OTP and
retain identity/projection execution. In step 9, execute same-name clause isolation
and failed-candidate rollback; keep these obligations open until dispatch exists.

### 5. Validate and normalize pattern semantics

- [ ] Add bounded private semantic pattern analysis consuming both
  `RestrictedPattern` and `PatternCandidate`; retain source anchors in its output.
- [ ] Normalize variables, literals, grouping, aliases/compound patterns and legal
  constant arithmetic. Distinguish expression `=` from compound-pattern `=`.
  Recognize later container forms without enabling missing runtime operations.
- [ ] Enforce context-specific legality, including map key expressions and binary
  size scopes. Do not allow one compound-pattern operand to supply a key/size
  binding to its sibling merely because lowering happens to visit it first.

**Success criteria:** Positive/negative fixtures agree with OTP on legality;
legal deferred forms get capability diagnostics, illegal forms get semantic errors.
Nested expressions in permissive pattern syntax cannot bypass validation. Deep or
large inputs hit documented budgets with no partial publication or host-stack crash.

**Tests:** Compare positive/negative cases from `match_SUITE.erl`,
`map_SUITE:t_key_expressions/1` and `bs_size_expr_SUITE.erl` with OTP. Include
illegal sibling key/size dependencies, nested invalid expressions and depth limits.

### 6. Implement immediate equality and matching (F12 slice)

- [ ] Introduce a private match plan with explicit test, extraction, binding,
  success and mismatch edges. Start with variables, wildcards, small-integer and
  atom and canonical empty-list/empty-tuple patterns, repeated names and aliases.
- [ ] Implement the F12 exact-equality slice for admitted immediate values. Make
  its extension point shared by patterns and exact guard comparisons; do not
  generalize raw-word equality to future boxed terms.
- [ ] Lower the plan into LLVM blocks with tentative SSA values and explicit
  continuations. Never perform an unchecked extraction or treat mismatch as an
  error inside the reusable matcher.

**Success criteria:** Real single-clause functions match/reject literals,
`f(X, X)`, aliases and wildcards at O0/O2; rejection reaches step 2's
`function_clause` path. Integer boundaries work for both target widths, and host
tests supply only values admitted by the real runtime. Existing direct calls and
argument identity behavior remain intact.

**Tests:** Compile selected/adapted `match_SUITE.erl` helpers for repeated variables,
aliases, wildcards and literal mismatch. Compare results/reasons with OTP; cover
both-width integer endpoints, owned atoms and nested generated-call failures.

### 7. Resolve guard calls and implement immediate services (F12/F26 slice)

- [ ] Validate legal operators/BIF name-and-arity pairs independently of executable
  support. Resolve explicit erlang calls, auto-imports, shadowing and admitted
  no_auto_import metadata. Keep legacy top-level guard tests distinct.
- [ ] Reject illegal calls/assignments even in unreachable branches. A generic
  builtin registration cannot authorize a guard call.
- [ ] Implement predicates and exact/numeric comparison/order over admitted values,
  sharing step 6 equality. Atom order uses spelling, not assigned IDs; term-valued
  booleans use step 3 atoms. Extend ordinary expression lowering as needed to
  exercise the same services through source.
- [ ] Separate semantic argument failures from allocation/resource/ownership,
  unavailable-service and internal failures; never map every non-OK status to false.

**Success criteria:** Invalid guards, wrong arities and legal unavailable services
remain distinct. Wrong-type behavior agrees with OTP. Incorrect specs cannot remove
required checks; runtime code remains LLVM-free.

**Tests:** Compile predicate/comparison kernels adapted from `guard_SUITE.erl` and
`beam_type_SUITE:numbers/1`; test cross-type inputs, boundaries and misleading specs.
Use `overridden_bif_SUITE.erl` for shadowing and qualified-call diagnostics.

### 8. Lower guard grouping and short-circuit behavior

- [ ] Preserve comma conjunctions and semicolon alternatives as separate control
  flow. Success requires Erlang `true`; false or non-boolean final values reject
  the relevant guard. Failed alternatives may continue at the next semicolon.
- [ ] Implement `andalso`/`orelse` with lazy right operands, separately from strict
  `and`/`or`/`xor` and `not`. Preserve term-valued intermediate results and validate
  operands where OTP requires booleans; do not flatten all forms into LLVM `i1`.
- [ ] Route a reached guard error to the enclosing guard failure continuation,
  including inside nested boolean expressions. It is not a replacement `false`
  operand that `orelse` may recover from. Use the atom values implemented in step 3.

**Success criteria:** Oracle/native tests distinguish `;` from `orelse`, prove
skipped operands are not evaluated, and exercise reached bad arguments, non-boolean
results, nested grouping and alternative recovery. O0/O2 and specialization on/off
agree; no guard body executes after a failing head pattern.

**Tests:** Compile selected helper clusters from `guard_SUITE.erl` and `andor_SUITE.erl`
that distinguish semicolon alternatives from orelse, skipped failing operands,
reached bad arguments and non-booleans. Rerun fallback-clause cases after step 9.

### 9. Integrate ordered function clauses (F15 slice)

- [ ] Extend capability analysis, binding analysis, call graph traversal, inference
  and lowering beyond their current first-clause assumptions. Inspect every body.
- [ ] Try clauses in source order: pattern, guard, then body. Carry original
  arguments into each attempt and discard tentative values on candidate failure.
- [ ] Route exhaustion to `function_clause`; preserve export and call resolution
  rules and conservative summaries. Keep recursion under its existing F21 gate.

**Success criteria:** Overlapping heads, a failed guard followed by fallback,
repeated-variable mismatch and complete exhaustion work through local and remote
calls. Later clauses cannot inherit earlier bindings. Unsupported code in a later
or unused clause is still diagnosed. Record this completed overlap under F15.

**Tests:** Execute overlapping heads, failed-guard fallback, repeated-variable mismatch
and exhaustion from selected `match_SUITE`/`guard_SUITE` helpers through local and
remote calls. Complete the execution obligations from steps 4 and 8.

### 10. Integrate body matches and sequences (F16 slice)

- [ ] Add ordered body sequences and expression matches using the same matcher.
  Evaluate the RHS once; matching returns that value and commits successful new
  bindings. Existing bindings are equality constraints, never assignments.
- [ ] Preserve right-to-left chained match semantics and distinguish parenthesized
  compound patterns. Propagate bindings made by the RHS according to OTP scope.
- [ ] Route body mismatch to `badmatch` with the RHS value; stop subsequent
  expressions and propagate failure through generated callers.

**Success criteria:** Real source executes `Y = X, Y`, rebinding checks,
chained matches and failing matches with OTP-equivalent results/reasons. A failed
match never runs later body work. Aliases preserve the matched value's identity and
lifetime. Record sequences/matches as the specific F16 contribution.

**Tests:** Compile selected/adapted `match_SUITE.erl` helpers for `Y = X, Y`, rebinding,
chained matches and mismatch. Compare values/reasons and show that a later failing
call is not reached after an earlier failed match.

### 11. Implement rooted, bounded heap construction (F02/F03 slice)

- [ ] Implement backing allocation, exact accounting, bounded growth, rollback
  and teardown. Connect TermFactory and host Terms to explicit lifetime/ownership.
- [ ] Register roots for generated arguments/temporaries, results and error payloads
  across allocating calls; implement cleanup and a documented safepoint contract.
  Version affected ABI layouts using target-derived widths.
- [ ] Use stable storage in this slice; garbage collection and graph copying stay
  outside this plan. Never relocate C++ resource objects as raw bytes or claim
  moving-GC survival without implementing and testing it.

**Success criteria:** Allocation failure leaks no partial values or roots. Live
values survive calls/growth, expired handles reject and failure payloads remain
owned. Compound admission waits for concrete lifetime tests in step 12.

**Tests:** Verify root/ABI cleanup through generated calls and allocation-failure
injection. In step 12, pass constructed heap values through unchanged OTP first/2
and id/1 while retaining earlier results across allocations and failed matches.
Immediate-only execution alone cannot prove heap lifetime correctness.

### 12. Construct, compare and match tuples/lists/strings (F08/F12)

- [ ] Implement immutable tuple/cons constructors, source construction and checked
  access BIFs. Extend structural equality/order with bounded traversal; independent
  equal allocations must compare by value.
- [ ] Use step 11 ownership and roots. Add checked
  tuple tag/arity tests and list cons/nil traversal, with no dereference before proof.
- [ ] Normalize strings and legal string-prefix patterns into list matching. Cover
  proper/improper lists, exact tuple arity, nested aliases and repeated variables.
- [ ] Root the candidate and extracted values across allocating calls/safepoints;
  commit bindings without reconstructing matched containers.

**Success criteria:** Real source constructs and matches nested containers;
wrong shapes and short/improper lists fail safely. Returned extracted terms survive
subsequent permitted allocation. Complete step 11 root/lifetime tests across heap growth and failed calls;
collection remains outside this plan.

**Tests:** Compile constructors/accessors adapted from `beam_type_SUITE` and
`bif_SUITE:head_tail/1`, then tuple/list patterns from `match_SUITE`. Cover short
and improper lists, exact arities, equal separate allocations and allocation failure.
Complete step 11 retained-value and rooted-error-payload tests.

### 13. Implement arbitrary integers and integer guards (F10/F12)

- [ ] Implement owned bignums/literals and small-integer promotion/demotion.
- [ ] Lower arithmetic, division/remainder, bitwise and shift operations with
  checked fast paths and runtime fallbacks; never wrap machine overflow.
- [ ] Extend literal/repeated-variable matching, exact/numeric comparison and
  guard operations. Define resource ceilings separately from Erlang failures.

**Success criteria:** Results are exact across signed 28/60-bit payload boundaries;
division by zero/wrong types produce the specified Erlang failure. Bignums are
rooted, independently allocated equal values compare exactly, and resource
failure never becomes an ordinary false guard.

**Tests:** Compile unchanged `trycatch_SUITE:my_div/2` and my_add/2. Adapt arithmetic
kernels from `beam_bounds_SUITE` without private BEAM helpers; compare negative
division/remainder, shifts, promotion/demotion, large repeated-variable patterns,
zero divisors and allocation failure. Guarded wrappers verify failure handling.

### 14. Implement floats, mixed comparisons and numeric guards (F11/F12)

- [ ] Implement float ownership/literals, arithmetic and required conversions,
  including checked mixed integer/float paths and rounding behavior.
- [ ] Extend literal/repeated-variable matching and exact/numeric comparison;
  integer/float exact equality stays distinct and mixed ordering avoids lossy casts.
- [ ] Preserve OTP error behavior and evaluation order; exclude unsafe LLVM
  fast-math assumptions and unsupported non-finite values.

**Success criteria:** Float operations and conversions agree with the pinned
semantic evidence and compatible oracle at boundaries. Invalid/overflow results
propagate through step 2; integer precision is not lost by general comparison casts.

**Tests:** Compile unchanged `float_SUITE:pc/3` once round/1 exists, and selected/adapted
`beam_type_SUITE:float_compare/1` cases. Compare signed zero, rounding ties, large
integer/float neighbors, overflow, wrong operands and repeated patterns with 1/1.0.

### 15. Implement maps and bound-key matching (F08/F12)

- [ ] Implement rooted construction, association/exact updates, exact-key lookup,
  map equality/order and checked size/key services. Preserve evaluation order;
  integer/float keys remain distinct and insertion order does not affect equality.
- [ ] Use checked exact-key services and implemented guard expressions.
  Evaluate legal key expressions in their defined incoming scope, preserving
  failures and excluding illegal bindings.
- [ ] Match every required `:=` association; allow extra keys. Treat `#{}` as a map
  type test, and retain all value constraints when key expressions resolve equally.
- [ ] Reuse rooted checked lookup rather than duplicating map layout knowledge in
  LLVM lowering. Keep key expression failure distinct from infrastructure failure.

**Success criteria:** OTP/native coverage includes extra/missing keys, duplicate
keys, exact integer/float key distinctions, nested values, computed bound keys and
illegal same-pattern key dependencies. Failed lookup leaves candidate bindings
unchanged. Equal maps need not share allocation identity to match repeated names.

**Tests:** Compile selected/adapted `map_SUITE` helpers from t_map_get/1, t_map_size/1,
t_update_exact/1, t_duplicate_keys/1 and t_key_expressions/1. Compare nested/compound
keys, extra/missing keys, duplicate constraints, illegal sibling bindings, badmap/
badkey and failed-construction cleanup with OTP.

### 16. Implement bitstring construction, extraction and matching (F09)

- [ ] Implement rooted small/shared immutable storage, exact bit lengths/tail rules
  and checked integer/float/UTF construction. Extend equality/order and size/part
  services before enabling their pattern/guard uses.
- [ ] Use checked extraction/ownership and numeric services. Validate
  segment types, defaults, units, signedness, endianness, UTF forms and tail rules.
- [ ] Track an explicit bit cursor; check type, size arithmetic and remaining bits
  before every read. Apply OTP rules for earlier segment bindings and size scopes,
  separately from sibling compound-pattern restrictions.
- [ ] Retain backing storage for extracted tails and root allocations. Share
  representation/extraction services with F09; use target semantics for native
  endianness rather than the compiler host's endianness.

**Success criteria:** Cases cover partial bytes, zero/truncated/invalid sizes,
signed fields, endian variants, UTF failures, dependent sizes, repeated variables
and retained tails after candidate teardown. Native and OTP results agree; foreign
object inspection is labeled separately from native execution.

**Tests:** Parse bs_construct_SUITE, bs_match_SUITE, bs_size_expr_SUITE,
bs_bit_binaries_SUITE and bs_utf_SUITE. Compile selected/adapted helpers for strings/1,
bad_size/1, zero_width/1, bin_tail/1, shared_sub_bins/1 and UTF literals/1. Cover
truncation, dependent sizes, endianness, UTF failures and retained-tail lifetime.

### 17. Expand records into tuple patterns and guard operations (F17 slice)

- [ ] Resolve included declarations, fields/defaults and record operations admitted
  in patterns/guards. Reuse tuple construction/access and record tag/arity checks.
- [ ] Normalize record patterns, including wildcard fields, preserving locations
  and OTP evaluation rules. Implement construction needed to exercise them;
  other record features remain capability-gated.

**Success criteria:** Record matching and guard checks agree on field positions and
shape. There is no independent record matcher. Missing declarations, invalid fields
and wrong-shaped access have correct diagnostics or guard failure behavior.

**Tests:** Parse `record_SUITE.erl` and record_SUITE_data/record_access_in_guards.erl.
Compile adapted record helpers from errors/1, eval_once/1 and nested_access/1 using
supported operations; test included declarations, tags/arities and default evaluation.
The full data module needs funs/comprehensions: do not claim full native coverage.

### 18. Complete guard services for admitted representations (F26 slice)

- [ ] Reconcile step 1's catalog with predicates, comparisons, numeric conversions,
  min/max, tuple/list/map/binary queries and record tests. Include is_integer/3.
- [ ] Implement missing in-scope signatures and guard construction/map-update forms
  through existing checked services, with bounded traversal/allocation and roots.
- [ ] Keep services requiring unavailable functions/identities/processes explicitly
  gated. Do not broaden this plan to implement their owners or admit forged terms.

**Success criteria:** Every in-scope signature has executable valid, invalid and
boundary coverage. Reached semantic errors reject guards; resource/internal failures
retain specified outcomes. Legal unavailable families remain clearly identified.

**Tests:** Compile selected/adapted kernels from bif_SUITE:trunc_and_friends/1,
min_max/1, map_SUITE:t_guard_bifs/1 and guard_SUITE:is_integer_3_guard/1, retaining
original guarded helper bodies where supported. Test constructors/map updates,
legacy tests, qualified calls and explicit unavailable-service diagnostics.

### 19. Verify inference and optimization across supported forms

- [ ] Replace argument-index-only assumptions in `semantic/types/inference.*` and
  `codegen/lowering_expressions.*`. Track bound/extracted values conservatively;
  join alternative results without leaking candidate-only facts.
- [ ] Keep implementation facts separate from specifications. Representation tests
  must dominate each dependent load/unbox; joins retain only common proven facts.
- [ ] Reuse the existing specialization budgets and generic fallback. Extend
  `integer_guards.*` only for checks whose removal is justified by dominating proof.

**Success criteria:** `--print-types` and both IR inspection modes handle new
syntax without missing-table crashes. Incorrect specs and adversarial inputs give
the same observable results in all optimization modes. LLVM verification passes
before/after transformations; focused IR checks establish safe access ordering.

**Tests:** Run paired annotated/unannotated OTP-derived kernels with wrong-spec inputs,
nested allocation/calls and failed candidates. Compare optimization/specialization
modes, inspect required check dominance, exercise budgets and run type/IR CLI modes.

### 20. Finish validation and publish the scoped contract

- [ ] Run the provenance-checked OTP helper corpus, bounded seeded regressions and
  fresh combined build/CTest/Lizard/clang-tidy gate. Exercise deep/wide inputs,
  many alternatives, work/IR limits, allocation failures and cleanup.
- [ ] Update affected semantic/compiler/runtime/ABI contracts, examples, capability
  coverage, .agents/arch.md, .agents/files.md, aimemory.md and delivered backlog
  slices. Preserve historical evidence and explicitly record native platform gaps.

**Success criteria:** Every in-scope matrix row has passing executable evidence;
parser-only results are labeled. Documentation and capability diagnostics agree.
Completing this plan does not imply support for every Erlang guard/source context
or close unrelated backlog features.

**Tests:** Run local/remote and positional/project workflows at O0/O2 with specialization
on/off, comparing values and error class/reason with OTP. Include failed publication,
ABI mismatch, post-failure recovery and documented examples. Execute available native
Windows, Linux and Apple Silicon workflows; label foreign-object checks separately.

## Implementation locations

Extend existing semantic/binding/type passes under `compiler/src/semantic/`, small
lowering helpers under `compiler/src/codegen/`, and existing ABI, term, heap,
builtin and module owners under `abi/` and `runtime/`. Reuse
`tests/compiler/codegen/{native.cmake,differential.py,execution_oracle.escript}`,
their fixtures and the pinned-source checks in `tests/compiler/parser/`.
No public interchange formats or intermediate-stage readers are introduced.
