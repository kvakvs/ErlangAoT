# F13 Pattern matching and F14 Guards — implementation plan

Created 2026-10-01 from [the feature backlog](01-todo.md). Necessary
prerequisites are included below in implementation order. Steps 1–20 and added step 15a are complete. Completion evidence is linked
under each finished step; the [final scoped validation](../docs/patternmatch-step20-validation.md)
records delivered contexts, remaining owners and unavailable native runners.

Each completed step should end with a commit, commit title will be "[compiler]
<step name>"

## Scope

Implement matching and guards in function clauses and body match expressions,
including atoms, numbers, tuples, lists/strings, maps, bitstrings and expanded
records. Include only the runtime construction, checked access, comparison,
ownership and failure services these forms need. Ordered function clauses and
body sequences/matches are the required F15/F16 slices.

Completed steps execute patterns, rooted construction/access, exact structural
comparisons and grouped/boolean guards over every admitted representation in
ordered-clause functions and body matches/sequences. The guard catalog, conservative
binding facts, optimization proof audit and final validation are complete. This
plan includes only the necessary runtime prerequisites; no separate prerequisite
plan is needed.

Other source contexts and runtime features remain in the backlog: case/if,
catch/try, funs, receive, process/port/reference services, recursion,
scheduling, message copying, garbage collection and production executable
linking. They are not completion requirements here. Guard services requiring
those features remain explicitly unavailable; predicates may still classify the
admitted value domain. Do not claim complete Erlang guard coverage or all
F13/F14 source contexts.

## Validation rules

Per user-added step 15a, routine tests use project-owned pregenerated goldens.
OTP extraction/execution below happens only during explicit regeneration or
opt-in upstream audits; neither OTP nor its checkout is a build/test prerequisite.

The official maint-29 head was re-fetched on 2026-10-03 and matched the checkout
and pin `21776803ecd11f5fa948732c0ec66b8f325dedfc`. The originally untracked
`lib/stdlib/src/1.ir` was removed by the user during step 4. At implementation
start, recheck upstream and follow
[otp-reference.md](../docs/otp-reference.md); synchronize pin, checkout, corpus
hashes and grammar evidence while preserving historical records. Record the
installed OTP oracle version separately. The step-1 record below supplies the
initial evidence.

Use pinned `system/doc/reference_manual/expressions.md`,
`lib/stdlib/src/{erl_lint,erl_internal}.erl` and compiler suites as evidence.
All suite names below refer to `references/otp/lib/compiler/test/`. Distinguish:

- Preprocess/parse original suite files with real includes and feature flags;
  parsing a suite is not executable coverage.
- Compile selected complete helpers or explicitly labeled adaptations with OTP
  and the public compiler CLI. Record source path, function/arity, revision,
  hash, declarations and adaptations; preserve license notices. Do not remove
  the behavior under test to fit the supported subset.
- Emit objects and execute them in the existing separately linked runtime
  consumer. Compare values, selected clauses and error class/reason, not raw IDs
  or unstable stack formatting. Unrelated Common Test harness code need not
  compile natively.

Every step must pass its success criteria and tests, including negative cases.
Run native workflows at O0/O2 with specialization on/off where applicable.
Preserve misleading-spec cases, positional/project modes, failed-batch
nonpublication and post-failure recovery. Keep focused invariant/fault tests
only where real source cannot practically establish ownership, budgets or
injected failure behavior. Unavailable tools and deferred cases cannot count as
passing evidence.

Preserve internal C++23 APIs, owned ASTs, source provenance, bounded traversal,
target-derived layouts and LLVM-free runtime code. Document field/function
intent in one or two lines and use clang-format. Before each clean
implementation commit, freshly configure build/debug with compiler, runtime and
BUILD_TESTING=ON; build, run CTest, then
`cmake --build build/debug --target check-quality`. Pass Lizard and clang-tidy
without relaxed thresholds or suppressions. Record native platform results
separately from foreign-object/32-bit layout checks and list missing runners.

## Ordered implementation steps

### 1. Fix the semantic matrix and OTP evidence

- [x] Refresh/check the reference using the documented procedure, preserving
  local work. Review pattern and guard rules against the pinned lint/compiler
  sources.
- [x] Create a matrix of pattern forms, guard operators/BIF name-and-arity
  pairs, the two admitted source contexts, legal-but-deferred features and
  invalid constructs. Include legacy guard tests, qualified BIFs and OTP 29
  additions such as `is_integer/3`; do not infer legality from the preprocessor
  evaluator's subset.
- [x] Seed small real `.erl` fixtures and an OTP oracle for acceptance, results,
  selected clause and error class/reason. Cover `_` versus `_Name`, repeated
  names, compound patterns, guard alternatives and invalid calls.

**Success criteria:** Every matrix row has an implementation step or an explicit
dependency owner and rejection expectation. Oracle fixtures record exact
source/oracle versions and distinguish syntax acceptance from semantic
acceptance. The initial slice and later completion boundary are reviewable
without guessing.

**Tests:** Parse `guard_SUITE.erl`, `match_SUITE.erl` and `trycatch_SUITE.erl`
with real includes. Compile unchanged `bif_SUITE:first/2` and `guard_SUITE:id/1`
on admitted immediates as a baseline. Record provenance and reject a
deliberately stale hash.

**Completed 2026-10-01:** [Semantic matrix](../docs/patternmatch-matrix.md), 81
source-checked signature rows, 14 acceptance modules and 40 OTP outcomes.
Original guard/match/trycatch suites parse with real headers; unchanged first/2
and id/1 execute through the separate native consumer in all four O0/O2 and
specialization modes. Stale source hashes reject before execution. Fresh Windows
x64 Debug: 104/104 CTests, zero skips, full Lizard/clang-tidy pass. See
[the validation record](../docs/patternmatch-step1-validation.md) for exact
versions, provenance and platform limits. No executable pattern/guard support or
step-2 failure transport is implemented by this step.

### 2. Implement generated-call failure propagation (F20/F02 slice)

- [x] Coordinate a minimal F20/F02 contract for successful results, clause
  mismatch, guard rejection, Erlang errors and runtime infrastructure failures.
  Decide the concrete transport before emitting fallible code: explicit
  status/result or a checked context error channel; document why the chosen
  scheme fits later F20.
- [x] Implement propagation through local/remote generated calls, runtime
  services, registration and native consumers. Version descriptors/signatures if
  their contract changes; retain target-derived layout and native C++ service
  linkage.
- [x] Preserve enough structured error information for `function_clause` and
  `badmatch` with its offending value, with rooted ownership where needed. Add
  heap payload ownership in step 11. Full catch/try is separate.

**Success criteria:** A failed nested generated call cannot become a valid term
or continue its caller's body. Wrong ABI consumers are rejected. Tests observe
success and structured failure separately, no C++ exception escapes the
boundary, and a later independent invocation has no stale failure state.
Ordinary mismatch and guard rejection are silent selection outcomes, not feature
diagnostics.

**Tests:** Execute baseline helpers across two generated modules. Use the
existing native service failure seam to verify propagation, cleanup and
successful retry. Steps 6/10 add source function_clause/badmatch; step 13 adds
real badarith.

**Completed 2026-10-01:**
[Revision-2 failure contract](../docs/generated-call-failures.md) uses a checked
context channel with first-failure ownership and outer-scope cleanup.
Local/remote generated calls stop before result use or later argument/body work;
registration and builtin/heap services preserve structured errors and exact
status. Revision-1 descriptors reject and startup requires the revision-2
runtime service. Four O0/O2/specialization fault workflows cover nested and
reentrant calls, function_clause/badmatch payload transport,
service/diagnostic/native failures and successful retry. Fresh Windows x64
Debug: 108/108 CTests, zero skips, full Lizard/clang-tidy pass. See
[validation](../docs/patternmatch-step2-validation.md). Source matching/guards
and heap payload roots remain with their later steps.

### 3. Implement atoms and boolean values (F06)

- [x] Implement runtime-owned stable atom storage, spelling validation,
  configured limits, deduplication and transactional module spelling/slot
  initialization.
- [x] Lower atom literals and `true`/`false` via module bindings; never bake in
  compiler-assigned IDs or intern on each expression evaluation.
- [x] Admit owned atoms through host Terms and error materialization; define
  cross-runtime rejection/remapping and preserve module lifetime rules.

**Success criteria:** Equal spellings in separate modules share identity within
one runtime. Foreign atom words cannot be mistaken for local atoms. Registration
failure leaves no published partial module and has a documented atom-table
policy.

**Tests:** Compile atom-return leaves adapted from `guard_SUITE.erl` and call
them across modules. Compare spellings and booleans with OTP; test Unicode,
deduplication, capacity failure and independent runtimes without comparing raw
atom IDs.

**Completed 2026-10-01:** [Runtime-owned atoms](../docs/runtime-atoms.md)
provide validated UTF-8 spelling, limits, deduplication, immutable host/error
pins and foreign-word rejection. Revision-3 descriptors publish per-runtime
spelling/slot bindings with their registry/image; generated literal reads never
intern or embed runtime IDs. Failed modules remain unpublished; retained valid
atoms count against the cap. OTP-adapted return leaves match all four native
optimization policies; Unicode, capacity, allocation-fault rollback, independent
runtimes and retry pass. Fresh Windows x64 Debug: 109/109 CTests, zero skips,
full Lizard/clang-tidy over 189 production units. See
[validation](../docs/patternmatch-step3-validation.md). Host serialization is
required; atom GC and worker synchronization remain deferred. At that
checkpoint, steps 4–20 had not been started.

### 4. Introduce scoped bindings and conservative value facts

- [x] Extend `semantic/declarations.hpp` and `bindings.*` with stable
  clause-local binding identities and explicit reads, definitions and
  already-bound checks. Preserve original argument provenance where applicable.
- [x] Represent incoming bindings and tentative candidate bindings separately.
  `_` creates no binding; `_Name` behaves as a normal name; repeated variables
  request exact equality. A successful pattern makes its bindings available to
  its guard; only a successful candidate makes them available to its body.
- [x] Define body-match scopes; guards may read bindings but cannot assign.
  Update inference, lowering and inspection consumers with conservative facts
  for new identities now; step 19 checks optimization across the complete value
  domain.

**Success criteria:** CLI diagnostics identify unbound/unsafe reads and `_`
reads with original locations. Existing identity/projection functions still
work. Two clauses using identical variable names have independent identities,
and failed candidates leave their incoming environment unchanged.

**Tests:** Compare binding legality from selected `match_SUITE.erl` cases with
OTP and retain identity/projection execution. In step 9, execute same-name
clause isolation and failed-candidate rollback; keep these obligations open
until dispatch exists.

**Completed 2026-10-01:** [Scoped bindings](../docs/scoped-bindings.md) provide
stable clause/local identities, explicit definitions/reads/exact checks,
separate incoming/tentative environments and success-only publication. RHS-first
matches, sibling visibility, read-only guards and short-circuit unsafe states
retain source locations; only whole original arguments retain projection facts.
Iterative walks have a shared budget and clear partial tables on exhaustion.
Twenty-six authored OTP legality cases, six unchanged match_SUITE helpers,
private identity/rollback invariants and native identity/projection execution in
all four policies pass. Fresh Windows x64 Debug: 111/111 CTests, zero skips,
full Lizard/clang-tidy over 191 production units. See
[validation](../docs/patternmatch-step4-validation.md). Executable
matching/guards remain gated; step 9 still owes same-name clause isolation and
failed-candidate rollback execution. Steps 5–20 remain pending.

### 5. Validate and normalize pattern semantics

- [x] Add bounded private semantic pattern analysis consuming both
  `RestrictedPattern` and `PatternCandidate`; retain source anchors in its
  output.
- [x] Normalize variables, literals, grouping, aliases/compound patterns and
  legal constant arithmetic. Distinguish expression `=` from compound-pattern
  `=`. Recognize later container forms without enabling missing runtime
  operations.
- [x] Enforce context-specific legality, including map key expressions and
  binary size scopes. Do not allow one compound-pattern operand to supply a
  key/size binding to its sibling merely because lowering happens to visit it
  first.

**Success criteria:** Positive/negative fixtures agree with OTP on legality;
legal deferred forms get capability diagnostics, illegal forms get semantic
errors. Nested expressions in permissive pattern syntax cannot bypass
validation. Deep or large inputs hit documented budgets with no partial
publication or host-stack crash.

**Tests:** Compare positive/negative cases from `match_SUITE.erl`,
`map_SUITE:t_key_expressions/1` and `bs_size_expr_SUITE.erl` with OTP. Include
illegal sibling key/size dependencies, nested invalid expressions and depth
limits.

**Completed 2026-10-02:** [Pattern semantics](../docs/pattern-semantics.md)
provides bounded flat normalization of both parser pattern categories, source
anchors, owned arithmetic constants and explicit compound-pattern constraints.
Map keys read incoming bindings; binary sizes additionally read their own
preceding segments, never sibling definitions. Embedded call/operator legality
and binary modifier checks remain independent of runtime capabilities. Any
semantic/budget failure clears all module binding/normalization tables.
Ninety-two authored OTP cases, unchanged match/binary helpers, map-key
adaptations, both CLI modes/four policies, 12,000-level private walks and
resource/nonpublication cases pass. Fresh Windows x64 Debug: 113/113 CTests,
zero skips, full Lizard/clang-tidy over 196 production units. See
[validation](../docs/patternmatch-step5-validation.md). Matching/guards remain
gated; record expansion/field validation remains step 17. Steps 6–20 have not
been started.

### 6. Implement immediate equality and matching (F12 slice)

- [x] Introduce a private match plan with explicit test, extraction, binding,
  success and mismatch edges. Start with variables, wildcards, small-integer and
  atom and canonical empty-list/empty-tuple patterns, repeated names and
  aliases.
  - [x] Consume step 5's normalized patterns and step 4's binding identities;
    retain source anchors and bound plan-node/work counts during construction.
  - [x] Define small plan operations and explicit candidate inputs/outputs;
    separate first bindings from repeated-name checks and share the input for
    aliases.
  - [x] Admit only the listed executable forms in capability analysis; keep
    later containers and numeric representations recognized but gated by their
    owners.
- [x] Implement the F12 exact-equality slice for admitted immediate values. Make
  its extension point shared by patterns and exact guard comparisons; do not
  generalize raw-word equality to future boxed terms.
  - [x] Define a checked equality contract for integers, owned atoms and
    canonical empty values, with separate unequal and runtime-failure outcomes.
  - [x] Derive integer tags/ranges from the target layout and compare atoms
    using the runtime bindings from step 3; reserve dispatch for later boxed
    values.
- [x] Lower the plan into LLVM blocks with tentative SSA values and explicit
  continuations. Never perform an unchecked extraction or treat mismatch as an
  error inside the reusable matcher.
  - [x] Map plan inputs and bindings to SSA values; pass success/mismatch blocks
    from the caller and make each access depend on its preceding representation
    test.
  - [x] Connect single-clause mismatch to the generated `function_clause`
    failure path, while retaining the matcher continuation for later
    clauses/body matches.
  - [x] Run the listed immediate kernels through the CLI/native consumer in all
    four policies; verify LLVM blocks and retain direct-call/projection
    regressions.

**Success criteria:** Real single-clause functions match/reject literals,
`f(X, X)`, aliases and wildcards at O0/O2; rejection reaches step 2's
`function_clause` path. Integer boundaries work for both target widths, and host
tests supply only values admitted by the real runtime. Existing direct calls and
argument identity behavior remain intact.

**Tests:** Compile selected/adapted `match_SUITE.erl` helpers for repeated
variables, aliases, wildcards and literal mismatch. Compare results/reasons with
OTP; cover both-width integer endpoints, owned atoms and nested generated-call
failures.

**Completed 2026-10-02:** [Immediate matching](../docs/immediate-matching.md) uses bounded normalized plans, tentative SSA bindings and checked shared runtime equality. Single-clause literal/repeated/alias/empty-value mismatch raises `function_clause`; unconditional heads retain compact projection IR. Thirty-four OTP/native calls pass all four policies and both CLI modes; 32/64-bit endpoint objects/IR, ownership, nested failures, plan limits and retry pass. Fresh Windows x64 Debug: 114/114 CTests, zero skips, full Lizard/clang-tidy over 199 production units. See [validation](../docs/patternmatch-step6-validation.md). Steps 7–20 remain pending.

### 7. Resolve guard calls and implement immediate services (F12/F26 slice)

- [x] Validate legal operators/BIF name-and-arity pairs independently of
  executable support. Resolve explicit erlang calls, auto-imports, shadowing and
  admitted no_auto_import metadata. Keep legacy top-level guard tests distinct.
  - [x] Reuse the signature catalog and embedded-expression resolution from step
    5; record resolved identity, guard legality and executable availability
    separately.
  - [x] Add located cases for qualified/unqualified calls, imports, local name
    collisions and suppression metadata, including legacy-test context
    restrictions.
- [x] Reject illegal calls/assignments even in unreachable branches. A generic
  builtin registration cannot authorize a guard call.
  - [x] Traverse every guard operand before lowering or constant folding;
    diagnose assignment, dynamic/user calls and invalid arities at their
    original locations.
  - [x] Keep runtime builtin lookup downstream of semantic authorization;
    exercise illegal calls behind constant short-circuit conditions through both
    CLI modes.
- [x] Implement predicates and exact/numeric comparison/order over admitted
  values, sharing step 6 equality. Atom order uses spelling, not assigned IDs;
  term-valued booleans use step 3 atoms. Extend ordinary expression lowering as
  needed to exercise the same services through source.
  - [x] List executable signatures for the current value domain and add checked
    runtime entry points, including tag classification for available
    representations.
  - [x] Implement immediate type ordering and spelling-based atom ordering;
    return canonical boolean atoms and share comparison logic between guards and
    bodies.
  - [x] Add cross-type and wrong-spec kernels; keep boxed numeric comparison
    extensions assigned to steps 13/14 instead of assuming raw-word ordering.
- [x] Separate semantic argument failures from allocation/resource/ownership,
  unavailable-service and internal failures; never map every non-OK status to
  false.
  - [x] Classify service outcomes explicitly and define which semantic failures
    become guard rejection versus Erlang errors in ordinary expression context.
  - [x] Preserve step 2's infrastructure-failure propagation; use the existing
    fault seam to prove that such failures cannot select a successful fallback
    result.

**Success criteria:** Invalid guards, wrong arities and legal unavailable
services remain distinct. Wrong-type behavior agrees with OTP. Incorrect specs
cannot remove required checks; runtime code remains LLVM-free.

**Tests:** Compile predicate/comparison kernels adapted from `guard_SUITE.erl`
and `beam_type_SUITE:numbers/1`; test cross-type inputs, boundaries and
misleading specs. Use `overridden_bif_SUITE.erl` for shadowing and
qualified-call diagnostics.

**Completed 2026-10-02:** [Immediate guard services](../docs/immediate-guards.md) resolve the pinned legal catalog independently of availability, including qualified/imported/shadowed/suppressed and legacy calls. Shared checked predicates, comparisons, spelling order and queries distinguish semantic badarg from exact infrastructure failures. All 1,689 OTP/native calls and 31 resolution cases pass both CLI modes/four policies; injected guard/body/nested faults, ownership, head-first execution, runtime-registration isolation, budget rollback and retry pass. Fresh Windows x64 Debug: 119/119 CTests and full 205-unit Lizard/clang-tidy. See [validation](../docs/patternmatch-step7-validation.md). Grouping/boolean control flow remains step 8; ordered clauses remain step 9.

### 8. Lower guard grouping and short-circuit behavior

- [x] Preserve comma conjunctions and semicolon alternatives as separate control
  flow. Success requires Erlang `true`; false or non-boolean final values reject
  the relevant guard. Failed alternatives may continue at the next semicolon.
  - [x] Lower each comma sequence with a shared rejection edge and route that
    edge to the next semicolon alternative, or the candidate mismatch
    continuation.
  - [x] Require canonical `true` at each guard-test boundary; preserve tentative
    pattern bindings for alternative reads without allowing guard definitions.
- [x] Implement `andalso`/`orelse` with lazy right operands, separately from
  strict `and`/`or`/`xor` and `not`. Preserve term-valued intermediate results
  and validate operands where OTP requires booleans; do not flatten all forms
  into LLVM `i1`.
  - [x] Give lazy operators separate right-operand blocks and term-valued joins;
    apply operand checks at the boundaries established by the semantic matrix.
  - [x] Lower strict boolean operators with the required operand evaluation and
    validation; share canonical atom conversion without reusing lazy control
    flow.
  - [x] Exercise skipped failing operands and non-boolean right-hand results in
    nested expressions, distinguishing intermediate terms from final guard
    tests.
- [x] Route a reached guard error to the enclosing guard failure continuation,
  including inside nested boolean expressions. It is not a replacement `false`
  operand that `orelse` may recover from. Use the atom values implemented in
  step 3.
  - [x] Thread the enclosing rejection continuation through nested lowering and
    keep semantic rejection separate from the generated-call failure exit.
  - [x] Pair reached-error `orelse` cases with semicolon recovery cases; first
    use single-clause exhaustion, then rerun with ordered fallback clauses in
    step 9.

**Success criteria:** Oracle/native tests distinguish `;` from `orelse`, prove
skipped operands are not evaluated, and exercise reached bad arguments,
non-boolean results, nested grouping and alternative recovery. O0/O2 and
specialization on/off agree; no guard body executes after a failing head
pattern.

**Tests:** Compile selected helper clusters from `guard_SUITE.erl` and
`andor_SUITE.erl` that distinguish semicolon alternatives from orelse, skipped
failing operands, reached bad arguments and non-booleans. Rerun fallback-clause
cases after step 9.

**Completed 2026-10-02:** [Guard control flow](../docs/guard-control-flow.md) preserves comma rejection and semicolon alternatives, canonical-true boundaries, eager strict operators, lazy RHS blocks and target-word SSA joins. Reached semantic errors reject the enclosing alternative; exact infrastructure failures stop all recovery. Body lazy-left errors retain OTP `{badarg, Value}` payloads. All 2,075 OTP/native calls, both CLI modes/four policies, 32/64-bit objects/IR, source stress and strengthened fault/retry/head-first workflows pass. Fresh Windows x64 Debug: 120/120 CTests and full 207-unit Lizard/clang-tidy. See [validation](../docs/patternmatch-step8-validation.md). Work stops after this separately committed step; ordered fallback and its reruns remain step 9.

### 9. Integrate ordered function clauses (F15 slice)

- [x] Extend capability analysis, binding analysis, call graph traversal,
  inference and lowering beyond their current first-clause assumptions. Inspect
  every body.
  - [x] Audit first-clause indexing and single-body assumptions in each
    consumer; iterate clauses in source order using their existing stable
    binding identities.
  - [x] Gather calls and capability diagnostics from every head, guard and body;
    conservatively join summaries and diagnose unsupported later/unused clauses.
- [x] Try clauses in source order: pattern, guard, then body. Carry original
  arguments into each attempt and discard tentative values on candidate failure.
  - [x] Build one entry per candidate and route head/guard rejection to the
    next; start each attempt from original arguments and an independent
    environment.
  - [x] Expose candidate bindings to its guard and commit them only on body
    entry; ensure no failed-candidate SSA value becomes an input to another
    clause.
  - [x] Execute overlapping heads, same-name clause bindings and guard fallback,
    closing the deferred execution checks from steps 4 and 8.
- [x] Route exhaustion to `function_clause`; preserve export and call resolution
  rules and conservative summaries. Keep recursion under its existing F21 gate.
  - [x] Use one final exhaustion block per function and the existing checked
    failure channel; leave export lookup and local/remote call identity
    unchanged.
  - [x] Exercise successful selection and exhaustion through local/remote
    callers; verify recovery on a later invocation and continued recursion
    diagnostics.

**Success criteria:** Overlapping heads, a failed guard followed by fallback,
repeated-variable mismatch and complete exhaustion work through local and remote
calls. Later clauses cannot inherit earlier bindings. Unsupported code in a
later or unused clause is still diagnosed. Record this completed overlap under
F15.

**Tests:** Execute overlapping heads, failed-guard fallback, repeated-variable
mismatch and exhaustion from selected `match_SUITE`/`guard_SUITE` helpers
through local and remote calls. Complete the execution obligations from steps 4
and 8.

**Completed 2026-10-02:** [Ordered clauses](../docs/ordered-clauses.md) use independent candidate SSA bindings, original arguments, head/guard fallback and one exhaustion exit. Every clause feeds capability/call/inference/atom/inspection analysis. Complete guard_SUITE fallback helpers and immediate match adaptations compare 1,020 calls with OTP in four policies and both CLI modes; later/unused diagnostics, recursive-call rejection, wrong specs, conservative result joins, 129 candidates and infrastructure-failure bypass/retry pass. Fresh Windows x64 Debug: 121/121 CTests, zero skips, full 208-unit Lizard/clang-tidy. See [validation](../docs/patternmatch-step9-validation.md). Steps 4/8 clause-isolation/fallback execution obligations are closed; steps 10–20 remain open.

### 10. Integrate body matches and sequences (F16 slice)

- [x] Add ordered body sequences and expression matches using the same matcher.
  Evaluate the RHS once; matching returns that value and commits successful new
  bindings. Existing bindings are equality constraints, never assignments.
  - [x] Lower sequence expressions in order, checking fallible operations before
    advancing; return the final expression's value and carry successful
    bindings.
  - [x] Save the RHS value once, invoke the match plan with the current
    environment, and publish only new bindings on success while returning the
    saved value.
- [x] Preserve right-to-left chained match semantics and distinguish
  parenthesized compound patterns. Propagate bindings made by the RHS according
  to OTP scope.
  - [x] Follow normalized expression/pattern categories for chained and compound
    matches; lower inner RHS matches before their enclosing expression match.
  - [x] Add paired source fixtures for chains and aliases, including RHS-created
    bindings and conflicts with already-bound names; compare legality and
    execution.
- [x] Route body mismatch to `badmatch` with the RHS value; stop subsequent
  expressions and propagate failure through generated callers.
  - [x] Supply a body-specific mismatch continuation that records the saved RHS
    in step 2's error contract; keep heap payload admission deferred until roots
    exist.
  - [x] Place a distinguishable failing call after a failed match and verify the
    original `badmatch` survives nested callers; also test success and later
    retry.

**Success criteria:** Real source executes `Y = X, Y`, rebinding checks, chained
matches and failing matches with OTP-equivalent results/reasons. A failed match
never runs later body work. Aliases preserve the matched value's identity and
lifetime. Record sequences/matches as the specific F16 contribution.

**Tests:** Compile selected/adapted `match_SUITE.erl` helpers for `Y = X, Y`,
rebinding, chained matches and mismatch. Compare values/reasons and show that a
later failing call is not reached after an earlier failed match.

**Completed 2026-10-02:** [Body matches and sequences](../docs/body-matches.md) reuse normalized matching with one saved RHS, exact existing-name constraints and success-only bindings. Chained RHS matches precede outer patterns; body mismatch retains the RHS in badmatch and stops later work/nested callers. 1,666 OTP/native calls in four policies and both CLI modes cover aliases, rebinding, RHS scopes, 128-match sequences/chains, wrong specs and first-failure recovery; real service counts prove single evaluation and skipped later work. Fresh Windows x64 Debug: 122/122 CTests, zero skips, full 209-unit Lizard/clang-tidy. See [validation](../docs/patternmatch-step10-validation.md). Heap roots and compound payloads remain steps 11–12; steps 11–20 remain open.

### 11. Implement rooted, bounded heap construction (F02/F03 slice)

- [x] Implement backing allocation, exact accounting, bounded growth, rollback
  and teardown. Connect TermFactory and host Terms to explicit
  lifetime/ownership.
  - [x] Specify allocation units, alignment, capacity limits and ownership
    handles; check size arithmetic before reserving stable backing storage.
  - [x] Publish a constructed value only after initialization succeeds; roll
    back partial reservations and accounting on every failure path.
  - [x] Connect factory/host admission to the owning runtime and lifetime
    checks; verify teardown, foreign ownership rejection and expired-handle
    behavior.
- [x] Register roots for generated arguments/temporaries, results and error
  payloads across allocating calls; implement cleanup and a documented safepoint
  contract. Version affected ABI layouts using target-derived widths.
  - [x] Define root registration, update and release operations plus allocation
    boundaries; document which caller/callee owns each live-value root.
  - [x] Emit root scopes for arguments and live temporaries, transfer
    result/error ownership before cleanup, and release scopes on success and
    failure exits.
  - [x] Update descriptors/consumers for changed layouts and reject incompatible
    versions; exercise target widths and injected root/allocation failures.
- [x] Use stable storage in this slice; garbage collection and graph copying
  stay outside this plan. Never relocate C++ resource objects as raw bytes or
  claim moving-GC survival without implementing and testing it.
  - [x] Choose storage growth that preserves published addresses and uses proper
    C++ construction/destruction for resource-owning objects.
  - [x] Document no-collection limits and keep retained compound results/error
    payloads as explicit step-12 acceptance obligations before container
    admission.

**Success criteria:** Allocation failure leaks no partial values or roots. Live
values survive calls/growth, expired handles reject and failure payloads remain
owned. Compound admission waits for concrete lifetime tests in step 12.

**Tests:** Verify root/ABI cleanup through generated calls and
allocation-failure injection. In step 12, pass constructed heap values through
unchanged OTP first/2 and id/1 while retaining earlier results across
allocations and failed matches. Immediate-only execution alone cannot prove heap
lifetime correctness.

**Completed 2026-10-02:** [Stable storage and generated roots](../docs/generated-roots.md)
provide bounded aligned backing, transactional reservations/accounting, explicit
resource destruction and lifetime checks. Revision-4 generated scopes retain
arguments/temporaries, clear rejected candidates, transfer results before release,
and restore nested depth after native exceptions. Resource/root allocation sweeps,
outer/nested entry faults, old-ABI rejection and recovery pass. Fresh Windows x64
Debug: 123/123 CTests, zero skips, full 213-unit Lizard/clang-tidy and formatting.
See [validation](../docs/patternmatch-step11-validation.md). Compound admission and
retained compound results/error payloads remain explicit step-12 obligations;
collection, graph copying and suspension remain outside this plan.

### 12. Construct, compare and match tuples/lists/strings (F08/F12)

- [x] Implement immutable tuple/cons constructors, source construction and
  checked access BIFs. Extend structural equality/order with bounded traversal;
  independent equal allocations must compare by value.
  - [x] Define tuple/cons layouts and checked construction services over step
    11; evaluate source elements in order and publish only fully initialized
    containers.
  - [x] Implement the accessors needed by the selected kernels with separate
    wrong-type/index failures and infrastructure failures.
  - [x] Add iterative equality/order worklists with explicit budgets and nested
    term dispatch; compare separately allocated equal containers through source.
- [x] Use step 11 ownership and roots. Add checked tuple tag/arity tests and
  list cons/nil traversal, with no dereference before proof.
  - [x] Extend match-plan operations for tuple shape/field extraction and cons
    head/tail extraction, routing wrong shapes and exhausted lists to mismatch.
  - [x] Lower loads only after dominating tag, arity and ownership checks;
    inspect representative IR and execute wrong-shape inputs through the native
    consumer.
- [x] Normalize strings and legal string-prefix patterns into list matching.
  Cover proper/improper lists, exact tuple arity, nested aliases and repeated
  variables.
  - [x] Reuse normalized character values to build list patterns/construction;
    preserve source anchors and avoid a separate string runtime representation.
  - [x] Add empty/short/prefix/improper-list cases and nested tuple/list
    aliases; use structural exact equality for repeated names containing heap
    values.
- [x] Root the candidate and extracted values across allocating
  calls/safepoints; commit bindings without reconstructing matched containers.
  - [x] Retain extracted heap values in binding/root slots and transfer roots at
    successful body entry; release tentative roots on mismatch or runtime
    failure.
  - [x] Complete step 11's retained-result/error tests with real constructed
    values, unchanged first/2 and id/1, heap growth, nested calls and injected
    failure cleanup.

**Success criteria:** Real source constructs and matches nested containers;
wrong shapes and short/improper lists fail safely. Returned extracted terms
survive subsequent permitted allocation. Complete step 11 root/lifetime tests
across heap growth and failed calls; collection remains outside this plan.

**Tests:** Compile constructors/accessors adapted from `beam_type_SUITE` and
`bif_SUITE:head_tail/1`, then tuple/list patterns from `match_SUITE`. Cover
short and improper lists, exact arities, equal separate allocations and
allocation failure. Complete step 11 retained-value and rooted-error-payload
tests.

Completion: [step-12 validation](../docs/patternmatch-step12-validation.md),
[retained evidence](../docs/patternmatch-step12-evidence.json) and
[container contract](../docs/container-matching.md). Windows x64: fresh combined
125/125 CTests, 4,801 OTP/native calls in four policies, retained results/errors,
root/fault/budget cleanup, seven foreign object targets, and full 221-unit quality
checks passed. No GC or cross-heap graph copying is claimed.

### 13. Implement arbitrary integers and integer guards (F10/F12)

- [x] Implement owned bignums/literals and small-integer promotion/demotion.
  - [x] Define canonical sign/magnitude storage and rooted factory services;
    normalize zero and values that fit the target's small-integer payload.
  - [x] Materialize normalized integer literals without host-width truncation;
    check literal/allocation limits and exercise both target payload boundaries.
- [x] Lower arithmetic, division/remainder, bitwise and shift operations with
  checked fast paths and runtime fallbacks; never wrap machine overflow.
  - [x] Emit checked small-integer paths and promote overflow to exact runtime
    operations, preserving operand evaluation order and generated failure
    checks.
  - [x] Implement signed division/remainder, bitwise and shift semantics from
    the recorded evidence; check zero divisors, invalid operands and excessive
    work.
  - [x] Execute boundary-crossing and promotion/demotion chains in all four
    modes; verify exact results and rollback when fallback allocation fails.
- [x] Extend literal/repeated-variable matching, exact/numeric comparison and
  guard operations. Define resource ceilings separately from Erlang failures.
  - [x] Extend shared numeric dispatch across small and large integers,
    including equality between independent allocations and nested container
    elements.
  - [x] Route arithmetic semantic errors according to body/guard context;
    propagate budget and allocation failures without converting them into guard
    rejection.
  - [x] Run arithmetic helpers and guarded wrappers for large literals, repeated
    variables, negative operands and zero divisors against the OTP oracle.

**Success criteria:** Results are exact across signed 28/60-bit payload
boundaries; division by zero/wrong types produce the specified Erlang failure.
Bignums are rooted, independently allocated equal values compare exactly, and
resource failure never becomes an ordinary false guard.

**Tests:** Compile unchanged `trycatch_SUITE:my_div/2` and my_add/2. Adapt
arithmetic kernels from `beam_bounds_SUITE` without private BEAM helpers;
compare negative division/remainder, shifts, promotion/demotion, large
repeated-variable patterns, zero divisors and allocation failure. Guarded
wrappers verify failure handling.

Completion: [step-13 validation](../docs/patternmatch-step13-validation.md),
[retained evidence](../docs/patternmatch-step13-evidence.json) and
[integer contract](../docs/integer-matching.md). Fresh Windows x64: 127/127
CTests, 16,065 OTP/native calls in four policies, seven foreign object targets,
full 231-unit quality checks and formatting passed. Canonical sign/magnitude
integers, checked double-width fast paths, promotion/demotion, exact comparisons,
rooted failure propagation and construction/arithmetic rollback are implemented.
Resource ceilings remain explicit infrastructure outcomes; no foreign native
execution or new sanitizer run is claimed. Steps 14–20 remain open.

### 14. Implement floats, mixed comparisons and numeric guards (F11/F12)

- [x] Implement float ownership/literals, arithmetic and required conversions,
  including checked mixed integer/float paths and rounding behavior.
  - [x] Define the supported float representation, rooted construction and
    literal conversion; validate representable results before publishing runtime
    values.
  - [x] Implement arithmetic and required round/truncate/conversion services
    using the numeric dispatcher, with explicit wrong-type and range failure
    handling.
- [x] Extend literal/repeated-variable matching and exact/numeric comparison;
  integer/float exact equality stays distinct and mixed ordering avoids lossy
  casts.
  - [x] Add float exact comparison to scalar/container matching and preserve the
    distinction between exact equality and numeric equality in the shared API.
  - [x] Implement mixed integer/float ordering without rounding arbitrary
    integers first; test large neighbors, signed zero and repeated patterns
    using 1 and 1.0.
- [x] Preserve OTP error behavior and evaluation order; exclude unsafe LLVM
  fast-math assumptions and unsupported non-finite values.
  - [x] Check arithmetic/conversion results and propagate the recorded Erlang
    failure for unsupported results; audit emitted LLVM floating-point flags.
  - [x] Compare rounding ties, overflow and wrong operands through bodies and
    guards at O0/O2; record any oracle/platform restrictions in the evidence.

**Success criteria:** Float operations and conversions agree with the pinned
semantic evidence and compatible oracle at boundaries. Invalid/overflow results
propagate through step 2; integer precision is not lost by general comparison
casts.

**Tests:** Compile unchanged `float_SUITE:pc/3` once round/1 exists, and
selected/adapted `beam_type_SUITE:float_compare/1` cases. Compare signed zero,
rounding ties, large integer/float neighbors, overflow, wrong operands and
repeated patterns with 1/1.0.

Completion: [step-14 validation](../docs/patternmatch-step14-validation.md),
[retained evidence](../docs/patternmatch-step14-evidence.json) and
[float contract](../docs/float-matching.md). Fresh Windows x64: 129/129 CTests,
14,436 OTP/native calls in four policies, both target widths, full 238-unit
Lizard/clang-tidy and formatting passed. Finite owned binary64 values, checked
arithmetic/conversions, exact/numeric comparisons and rooted failure handling
are implemented. Other native runners and new sanitizer runs remain unclaimed.

### 15. Implement maps and bound-key matching (F08/F12)

- [x] Implement rooted construction, association/exact updates, exact-key
  lookup, map equality/order and checked size/key services. Preserve evaluation
  order; integer/float keys remain distinct and insertion order does not affect
  equality.
  - [x] Define immutable map storage and staged construction/update services;
    root keys/values while evaluating entries and roll back failed construction.
  - [x] Use exact term equality for key identity and implement
    missing-key/non-map outcomes for lookup and exact update separately from
    allocation failures.
  - [x] Implement bounded equality/order independent of insertion history; cover
    nested keys/values, duplicate updates and distinct integer/float keys.
- [x] Use checked exact-key services and implemented guard expressions. Evaluate
  legal key expressions in their defined incoming scope, preserving failures and
  excluding illegal bindings.
  - [x] Lower normalized key expressions against the recorded incoming binding
    environment; keep sibling pattern definitions unavailable to those reads.
  - [x] Evaluate and root each key as required by its source semantics; retain
    the pattern-context failure continuation around any fallible key
    computation.
- [x] Match every required `:=` association; allow extra keys. Treat `#{}` as a
  map type test, and retain all value constraints when key expressions resolve
  equally.
  - [x] Emit a map type test followed by required-key lookups and recursive
    value plans; do not require the candidate map's size to equal the pattern's
    size.
  - [x] Preserve separate value constraints for duplicate/equal computed keys;
    test extra/missing keys, contradictory constraints and empty-map patterns.
- [x] Reuse rooted checked lookup rather than duplicating map layout knowledge
  in LLVM lowering. Keep key expression failure distinct from infrastructure
  failure.
  - [x] Pass lookup results through checked service interfaces and root
    extracted values across later key computations and nested matches.
  - [x] Verify mismatch discards candidate bindings/roots, semantic errors
    follow the correct context, and injected failures propagate with successful
    later retry.

**Success criteria:** OTP/native coverage includes extra/missing keys, duplicate
keys, exact integer/float key distinctions, nested values, computed bound keys
and illegal same-pattern key dependencies. Failed lookup leaves candidate
bindings unchanged. Equal maps need not share allocation identity to match
repeated names.

**Tests:** Compile selected/adapted `map_SUITE` helpers from t_map_get/1,
t_map_size/1, t_update_exact/1, t_duplicate_keys/1 and t_key_expressions/1.
Compare nested/compound keys, extra/missing keys, duplicate constraints, illegal
sibling bindings, badmap/ badkey and failed-construction cleanup with OTP.

Completion: [step-15 validation](../docs/patternmatch-step15-validation.md),
[retained evidence](../docs/patternmatch-step15-evidence.json) and
[map contract](../docs/map-matching.md). Fresh Windows x64: 131/131 CTests,
8,010 OTP/native calls in four policies, both target widths, full 244-unit
quality checks and formatting passed. Immutable rooted maps, exact keys,
source-ordered updates, computed-key patterns and error payloads are implemented.

### 15a. Retain project-owned OTP golden fixtures (added by user)

- [x] Pregenerate source modules, inputs and expected values/errors using OTP;
  retain license notices, reference/oracle versions, hashes and adaptations.
- [x] Make routine building/testing independent of an OTP installation/checkout.
  Keep explicit regeneration and optional upstream audits separate from tests.
- [x] Validate stored expectations through real native workflows, run the fresh
  combined build/CTest/quality gate, and commit this step separately from 14/15.

Completion: [step-15a validation](../docs/patternmatch-step15a-validation.md)
and [evidence](../docs/patternmatch-step15a-evidence.json). Fourteen project-owned
corpora retain 49,959 expected native results plus semantic records. Fresh combined
OTP-free build: 118/118 tests, full 244-unit quality gate. Live audits are opt-in;
explicit OTP regeneration reproduces all retained inputs/results. Separate commit.

### 16. Implement bitstring construction, extraction and matching (F09)

- [x] Implement rooted small/shared immutable storage, exact bit lengths/tail
  rules and checked integer/float/UTF construction. Extend equality/order and
  size/part services before enabling their pattern/guard uses.
  - [x] Define owned backing buffers and bit-offset/length views with checked
    size arithmetic; distinguish byte-aligned binaries from general bitstrings.
  - [x] Add staged segment builders using integer/float services and UTF
    validation; publish only complete values and clean up partial buffers after
    failure.
  - [x] Implement bit-accurate equality/order and the required checked queries;
    test partial final bytes and independent buffers holding equal bit
    sequences.
- [x] Use checked extraction/ownership and numeric services. Validate segment
  types, defaults, units, signedness, endianness, UTF forms and tail rules.
  - [x] Consume step 5's normalized segment metadata and source anchors; gate
    runtime support by implemented segment form without duplicating legality
    rules.
  - [x] Add extraction services for admitted integer/float/binary/UTF segments;
    classify truncation/invalid encoding separately from resource or ownership
    faults.
- [x] Track an explicit bit cursor; check type, size arithmetic and remaining
  bits before every read. Apply OTP rules for earlier segment bindings and size
  scopes, separately from sibling compound-pattern restrictions.
  - [x] Carry candidate length and cursor through the match plan; compute
    segment width with overflow checks and advance only after successful checked
    extraction.
  - [x] Evaluate size expressions with incoming and permitted earlier-segment
    bindings; preserve separate sibling scopes and candidate rollback on
    failure.
  - [x] Exercise zero/truncated/invalid sizes, dependent lengths, repeated
    variables and tail constraints through source-generated function and body
    matches.
- [x] Retain backing storage for extracted tails and root allocations. Share
  representation/extraction services with F09; use target semantics for native
  endianness rather than the compiler host's endianness.
  - [x] Give tail views retained backing ownership and transfer roots on
    successful extraction; release failed-candidate views without invalidating
    returned tails.
  - [x] Derive native-endian lowering from target data and compare explicit
    endian variants; label cross-target object inspection separately from
    executed checks.
  - [x] Retain extracted tails across later allocations, caller return and
    candidate cleanup; inject construction/extraction allocation failures and
    verify recovery.

**Success criteria:** Cases cover partial bytes, zero/truncated/invalid sizes,
signed fields, endian variants, UTF failures, dependent sizes, repeated
variables and retained tails after candidate teardown. Native and OTP results
agree; foreign object inspection is labeled separately from native execution.

**Tests:** Parse bs_construct_SUITE, bs_match_SUITE, bs_size_expr_SUITE,
bs_bit_binaries_SUITE and bs_utf_SUITE. Compile selected/adapted helpers for
strings/1, bad_size/1, zero_width/1, bin_tail/1, shared_sub_bins/1 and UTF
literals/1. Cover truncation, dependent sizes, endianness, UTF failures and
retained-tail lifetime.

Completion: [step-16 validation](../docs/patternmatch-step16-validation.md),
[retained evidence](../docs/patternmatch-step16-evidence.json) and
[bitstring contract](../docs/bitstring-matching.md). Fresh Windows x64 OTP-free
combined build: 120/120 CTests, 8,826 golden calls per policy in four native
policies, both CLI modes, 11 added semantic cases and all 253 quality units pass.
Five suites parse; three target families have separate object-header/symbol
inspection. Rooted small/shared construction, numeric/UTF extraction, explicit
cursors, retained tails, size/part queries and bit-accurate comparison are delivered.
Records, remaining guards, optimization and finalization remain steps 17–20.

### 17. Expand records into tuple patterns and guard operations (F17 slice)

- [x] Resolve included declarations, fields/defaults and record operations
  admitted in patterns/guards. Reuse tuple construction/access and record
  tag/arity checks.
  - [x] Build record layouts from preprocessed declarations with field
    positions, defaults and source locations; diagnose duplicate/unknown
    declarations or fields.
  - [x] Resolve admitted access/test operations to the shared tuple services
    with tag/arity checks and context-appropriate failure outcomes.
- [x] Normalize record patterns, including wildcard fields, preserving locations
  and OTP evaluation rules. Implement construction needed to exercise them;
  other record features remain capability-gated.
  - [x] Expand record heads into tag-plus-field tuple constraints; distinguish
    omitted pattern fields and wildcard-field expansion from construction
    defaults.
  - [x] Lower supported construction with the recorded default/evaluation rules;
    preserve single evaluation and locations for explicit fields and nested
    access.
  - [x] Execute included-declaration, wrong-tag/arity, default and nested-access
    fixtures through tuple matching; retain diagnostics for unsupported record
    forms.

**Success criteria:** Record matching and guard checks agree on field positions
and shape. There is no independent record matcher. Missing declarations, invalid
fields and wrong-shaped access have correct diagnostics or guard failure
behavior.

**Tests:** Parse `record_SUITE.erl` and
record_SUITE_data/record_access_in_guards.erl. Compile adapted record helpers
from errors/1, eval_once/1 and nested_access/1 using supported operations; test
included declarations, tags/arities and default evaluation. The full data module
needs funs/comprehensions: do not claim full native coverage.

Completion: [step-17 validation](../docs/patternmatch-step17-validation.md),
[evidence](../docs/patternmatch-step17-evidence.json) and
[record contract](../docs/record-matching.md). Native Windows x64 passed
121 CTests and all 257 production quality units; 1,025 owned OTP outcomes run
in four policies. Native/qualified records, updates and record_info remain gated.

### 18. Complete guard services for admitted representations (F26 slice)

- [x] Reconcile step 1's catalog with predicates, comparisons, numeric
  conversions, min/max, tuple/list/map/binary queries and record tests. Include
  is_integer/3.
  - [x] Audit every catalog signature against its semantic resolver, runtime
    owner, lowering entry point and existing fixture; list concrete gaps by
    representation.
  - [x] Update the matrix with implemented versus dependency-blocked signatures,
    keeping legacy aliases, qualified forms and arity-specific cases
    identifiable.
- [x] Implement missing in-scope signatures and guard construction/map-update
  forms through existing checked services, with bounded traversal/allocation and
  roots.
  - [x] Fill catalog gaps using shared numeric/container services; avoid
    separate guard-only representations or unchecked access paths.
  - [x] Lower admitted constructors and map updates in guards with rooted
    temporaries and semantic-rejection continuations around each fallible
    operation.
  - [x] Add valid, wrong-type and boundary cases per missing signature,
    including is_integer/3, then exercise nested allocation and reached failures
    in alternatives.
- [x] Keep services requiring unavailable functions/identities/processes
  explicitly gated. Do not broaden this plan to implement their owners or admit
  forged terms.
  - [x] Link each deferred signature to its backlog dependency and keep
    legal-but- unavailable diagnostics distinct from illegal call or arity
    diagnostics.
  - [x] Test gates through qualified/unqualified and unreachable guard
    expressions; keep host inputs limited to values that the runtime can
    actually construct/admit.

**Success criteria:** Every in-scope signature has executable valid, invalid and
boundary coverage. Reached semantic errors reject guards; resource/internal
failures retain specified outcomes. Legal unavailable families remain clearly
identified.

**Tests:** Compile selected/adapted kernels from bif_SUITE:trunc_and_friends/1,
min_max/1, map_SUITE:t_guard_bifs/1 and guard_SUITE:is_integer_3_guard/1,
retaining original guarded helper bodies where supported. Test constructors/map
updates, legacy tests, qualified calls and explicit unavailable-service
diagnostics.

Completion: [step-18 validation](../docs/patternmatch-step18-validation.md),
[catalog evidence](../docs/patternmatch-step18-evidence.json) and
[guard contract](../docs/guard-services.md). All 81 rows are audited; four
signatures retain explicit owners. 5,033 native outcomes and 22 semantic cases
pass; the fresh gate passes 122 tests and all 257 quality units.

### 19. Verify inference and optimization across supported forms

- [x] Replace argument-index-only assumptions in `semantic/types/inference.*`
  and `codegen/lowering_expressions.*`. Track bound/extracted values
  conservatively; join alternative results without leaking candidate-only facts.
  - [x] Audit fact lookups for clause-local, body-created and extracted
    bindings; make missing/unproven information yield conservative facts rather
    than crashes.
  - [x] Track facts at successful match/guard edges and joins, dropping facts
    from failed candidates; preserve original-argument provenance only where
    justified.
  - [x] Exercise `--print-types` and both IR modes for every admitted
    representation, including multiple clauses, sequences and nested
    extractions.
- [x] Keep implementation facts separate from specifications. Representation
  tests must dominate each dependent load/unbox; joins retain only common proven
  facts.
  - [x] Audit optimization consumers so declared specs cannot authorize
    unchecked operations; derive removable checks from actual control-flow
    proofs.
  - [x] Add focused IR checks for tag/shape/check dominance and pair them with
    adversarial wrong-spec native inputs and failed-candidate regression
    fixtures.
- [x] Reuse the existing specialization budgets and generic fallback. Extend
  `integer_guards.*` only for checks whose removal is justified by dominating
  proof.
  - [x] Charge new match/guard paths against existing variant/work/IR budgets;
    retain a semantically equivalent generic path when specialization is
    declined.
  - [x] Run annotated/unannotated kernels in all four policies, verify LLVM
    before and after optimization, and exercise budget fallback without partial
    publication.

**Success criteria:** `--print-types` and both IR inspection modes handle new
syntax without missing-table crashes. Incorrect specs and adversarial inputs
give the same observable results in all optimization modes. LLVM verification
passes before/after transformations; focused IR checks establish safe access
ordering.

**Tests:** Run paired annotated/unannotated OTP-derived kernels with wrong-spec
inputs, nested allocation/calls and failed candidates. Compare
optimization/specialization modes, inspect required check dominance, exercise
budgets and run type/IR CLI modes.

Completion: [step-19 validation](../docs/patternmatch-step19-validation.md),
[proof evidence](../docs/patternmatch-step19-evidence.json) and
[binding facts](../docs/binding-facts.md). 822 paired native outcomes and 976 CFG
observations pass; both widths handle type/IR inspection. The fresh gate passes
123 tests and all 258 quality units; budgets and generic fallback are unchanged.

### 20. Finish validation and publish the scoped contract

- [x] Run the provenance-checked OTP helper corpus, bounded seeded regressions
  and fresh combined build/CTest/Lizard/clang-tidy gate. Exercise deep/wide
  inputs, many alternatives, work/IR limits, allocation failures and cleanup.
  - [x] Reconcile fixture provenance, adaptations, hashes and expected outcomes;
    map each in-scope matrix row to executable evidence and resolve coverage
    gaps.
  - [x] Run positional/project and local/remote workflows across all four
    policies, including failed publication, ABI rejection, recovery and bounded
    stress cases.
  - [x] Format changed code, freshly configure compiler/runtime with testing
    enabled, build, run CTest and run `check-quality`; retain logs without
    relaxed checks.
  - [x] Execute available native platform workflows and record missing runners;
    keep parser-only, foreign-object and layout evidence separate from native
    results.
- [x] Update affected semantic/compiler/runtime/ABI contracts, examples,
  capability coverage, .agents/arch.md, .agents/files.md, aimemory.md and
  delivered backlog slices. Preserve historical evidence and explicitly record
  native platform gaps.
  - [x] Describe final binding, matching, guard-error, representation and
    ownership contracts; align capability diagnostics and run the documented
    examples.
  - [x] Update compact architecture/file maps and memory, and close only the
    delivered F13/F14 and prerequisite backlog slices with links to validation.
  - [x] Publish the final validation record with exact source/oracle/tool
    versions, supported contexts, remaining dependencies and platform limits;
    retain older records.

**Success criteria:** Every in-scope matrix row has passing executable evidence;
parser-only results are labeled. Documentation and capability diagnostics agree.
Completing this plan does not imply support for every Erlang guard/source
context or close unrelated backlog features.

**Tests:** Run local/remote and positional/project workflows at O0/O2 with
specialization on/off, comparing values and error class/reason with OTP. Include
failed publication, ABI mismatch, post-failure recovery and documented examples.
Execute available native Windows, Linux and Apple Silicon workflows; label
foreign-object checks separately.

Completion: [step-20 validation](../docs/patternmatch-step20-validation.md),
[final evidence](../docs/patternmatch-step20-evidence.json) and
[scoped semantic matrix](../docs/patternmatch-matrix.md). All 19 corpora reproduce
against OTP; 67,634 native outcomes run in all eight driver/policy combinations.
The seeded closure corpus, examples, fault/limit/publication/recovery workflows
pass. Fresh Windows x64 Debug: 124/124 CTests, all 258 quality units, no weakened
checks. Other native hosts and new sanitizer coverage remain explicit gaps.

## Implementation locations

Extend existing semantic/binding/type passes under `compiler/src/semantic/`,
small lowering helpers under `compiler/src/codegen/`, and existing ABI, term,
heap, builtin and module owners under `abi/` and `runtime/`. Reuse
`tests/compiler/codegen/{native.cmake,differential.py,execution_oracle.escript}`,
their fixtures and the pinned-source checks in `tests/compiler/parser/`. No
public interchange formats or intermediate-stage readers are introduced.
