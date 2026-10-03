# Remaining compiler and runtime work — implementation plan 11

Created 2026-10-03 from [the feature backlog](01-todo.md). This is a planning
document: all new steps remain unchecked and no implementation is started by
creating it. The completed pattern/guard plan has been retired; its enduring
contracts, validation rules and evidence pointers are retained below and in the
[completed-work archive](00-finished.md#completed-patternmatch).

## Scope, ordering and completion

Implement the remaining required compiler/runtime features in dependency order,
so selected existing pure Erlang projects can become native executables with
isolated memory, collection, resumable processes and message passing. This plan
expands every backlog feature that still has unchecked work. F15 ordered clauses
is already delivered and remains a regression obligation, not a new
implementation.

Steps 1–39 are mandatory implementation/validation work; step 51 is final
closure. Steps 40–50 expand the optional D-items as conditional work. Record a
concrete selection, deferral or deliberate omission before their implementation.
Creating this plan does not select every optional feature. If an optional
feature is selected for this milestone, its implementation and tests also
precede step 51. Check a conditional decision off only with its recorded
disposition; never label a deferred/omitted implementation as delivered. D05
remains directory reservations only under AGENTS.md, even though its broader
future backlog is retained.

Numbering is a default execution order, not a claim that feature IDs specify
priority. Dependencies are explicit; an independent step can move earlier after
its prerequisites pass. Native runners and sanitizer toolchains should be
arranged during step 1, with feature tests run on them as the features arrive;
steps 35–37 close the accumulated matrix. Missing runners leave those validation
steps open. Each feature step includes its own integration tests and the common
gate; step 26 adds cross-feature stress after messaging exists, rather than
delaying ownership validation until then.

Each expanded step has second-level checklists for actual actions, observable
success criteria and required tests. Finish and validate each implementation
step in a separate commit with title `[compiler] <step name>`. A design/decision
step must publish a concrete reviewed contract or prototype and its evidence
before dependent code is enabled. Update backlog checkboxes only for the slice
delivered.

## Rules carried forward from plan 10

- Reuse project-internal C++23 APIs, owned stage data and the separately built
  LLVM-free runtime. Keep warnings as errors; document new function/field intent
  in one or two lines, keep complexity low and clang-format changed C++.
- Before each clean implementation commit, freshly configure `build/debug` with
  compiler, runtime and `BUILD_TESTING=ON`; build, run full CTest and
  `cmake --build build/debug --target check-quality`. Both Lizard and clang-tidy
  must pass without raised thresholds or suppressions to bypass findings.
- At the start of OTP-dependent implementation, follow
  [the reference procedure](../docs/otp-reference.md): fetch official
  `maint-29`, review advances and synchronize pin, clean checkout, corpus/source
  hashes, grammar witnesses and current docs. Record the installed oracle
  separately. Preserve historical revisions and local changes; never
  fetch/refresh in normal configure/build/test. This planning edit itself does
  not refresh the reference.
- Tests own their sources and expected results. Extract/regenerate only as an
  explicit reviewed action; retain license notices, exact source/function/arity,
  revision, source/generated/result hashes, oracle version and adaptations.
  Ordinary builds/tests require neither OTP nor its checkout; live audits remain
  opt-in. A changed result must be explained, not regenerated merely to pass.
- Prefer real public CLI/project/native executable workflows and black-box OTP
  comparisons over private unit tests or mocks. Keep focused tests for budgets,
  invalid handles/IR, ownership and injected failures that source cannot
  exercise. Do not delete useful tests before equivalent behavioral coverage
  exists.
- For applicable features, test positional/project drivers, local/remote paths,
  O0/O2 with specialization on/off, wrong/missing specs, invalid/unreachable
  syntax, resource limits, failed publication and recovery. Verify LLVM before
  and after transforms. Count outcomes once per documented workflow.
- Compare values, selected paths and stable error class/reason; use controlled
  process handshakes for ordering/races and avoid fragile exact-time assertions.
  Syntax-only suite parsing, upstream-only Common Test runs, foreign objects,
  32/64-bit IR/layout checks and native execution are distinct evidence.
- New representations must have construction, admission, comparison, trace,
  copy, destruction and error ownership rules before source enablement. Never
  admit forged identities or relocate resource-bearing C++ objects as raw bytes.
  New suspension/worker paths must publish every live root before use.
- Preserve legality versus availability versus runtime failure distinctions.
  Invalid source is diagnosed even when unreachable; missing owner services stay
  explicit capabilities. Guard semantic errors reject their alternative, while
  infrastructure/ownership/resource errors follow the defined failure path.
- Update contracts, capability tables, examples, `00-finished.md`, `arch.md`,
  `files.md` and `aimemory.md` with exact scope and evidence. Record missing
  native runners/tooling as gaps; historical evidence never proves a new
  implementation.

<a id="completed-patternmatch"></a>

## Retained context from the completed pattern/guard plan

Old plan 10 steps 1–20 and added 15a finished on 2026-10-03. Their numbering is
historical and independent of the new step numbers below. All per-step
completion records and gate counts are in [the archive](00-finished.md);
detailed validation files remain
`docs/patternmatch-step{1..20,15a}-validation.md` with their original dates,
revisions and qualifications. Deleting the completed checklist does not delete
those records or reset any implemented feature.

The current executable baseline supports ordered function clauses and their
guards, body matches/sequences, and acyclic direct local/exported remote calls.
Admitted terms are owned atoms/booleans, arbitrary integers, finite binary64,
tuples, proper/improper lists, strings, exact-key maps, bitstrings and ordinary
tuple records. Records currently support declarations/defaults, construction,
access, patterns and tests; updates/record_info/native/qualified/inferred forms
are new work. Function values, process/port/reference representations, GC, graph
copying, process execution and production executable linking are not yet
delivered at this baseline.

### Semantic and ownership invariants to preserve

- The shared bounded match plan has explicit test/extraction/binding/success/
  mismatch edges. Keep original arguments, isolated candidate SSA and fresh
  clause-local binding identities; head/guard rejection never publishes them.
  Ordered exhaustion raises function_clause and body mismatch owns its RHS.
- `_` creates no readable binding; `_Name` is ordinary. Repeated names use exact
  equality. Body matches evaluate RHS once and right-to-left chains first,
  publish only on success and return that saved value. Aliases constrain the
  same value and preserve its identity; they do not assign between siblings.
- Map keys read incoming bindings; binary segment sizes additionally read their
  own preceding segments. Pattern siblings cannot create each other's key/size
  bindings. Retain source/macro/include anchors and bounded traversal rollback.
- Guard legality comes from the exact pinned name/arity/operator catalog and
  lint rules, independently of preprocessor evaluation or runtime registration.
  Preserve qualified, shadowed/imported/suppressed and legacy resolution, and
  inspect skipped operands for legality. Comma/semicolon, strict boolean and
  lazy term-valued operators retain their distinct order/failure semantics.
- Structural comparisons use decoded values and exact map keys; boxed addresses
  do not determine equality. Preserve arbitrary-integer promotion/demotion,
  finite-float conversion and signed-zero rules, target-derived binary segment
  endianness and ordinary-record declaration-order evaluation.
- Current descriptors are ABI revision 4; the checked first-error channel is
  revision 2. Every fallible call checks before consuming output or continuing
  evaluation. Preserve owned errors, nested first-failure behavior, outer
  cleanup and independent retry; new handlers must explicitly consume the right
  failure.
- Stable indexed heaps prove ownership before extraction. Generated scopes root
  arguments/temporaries, release failed candidates, and hand off result/error
  ownership before pop. Host handles retain backing and deny expired access.
  Shared bitstring buffers/tails and other C++ resources have explicit
  lifetimes. Current heap stability does not establish future tracing, movement
  or copying.
- Implementation facts remain separate from specs. Whole-value body definitions
  may preserve proven argument relations; extracted/unproved values stay
  unknown, and joins retain only common proof. Shape/service-success checks
  dominate every dependent access. Existing specialization limits are 3
  variants/function, 32/module, 128/target and at most 2x measured generic IR;
  keep generic fallback, shared inference/work budgets and whole-draft rollback.
- Construction and semantic failure clear partial state; target batch
  publication stages complete files and preserves prior valid outputs on
  failure. Current multi-file replacement is not an atomic transaction. Preserve
  these limits explicitly when adding executables, project graphs and caches.

| Retained contract | Evidence and implementation guidance |
| --- | --- |
| Matrix, source evidence and signature authorization | [Semantic matrix](../docs/patternmatch-matrix.md), [guard services](../docs/guard-services.md), [step 18](../docs/patternmatch-step18-validation.md) |
| Bindings, normalized patterns and conservative facts | [Scopes](../docs/scoped-bindings.md), [patterns](../docs/pattern-semantics.md), [facts](../docs/binding-facts.md), [step 19](../docs/patternmatch-step19-validation.md) |
| Heads, grouping, ordered selection and body matching | [Immediate matching](../docs/immediate-matching.md), [guard control](../docs/guard-control-flow.md), [clauses](../docs/ordered-clauses.md), [body matches](../docs/body-matches.md) |
| Checked failures, atoms and roots | [Failure channel](../docs/generated-call-failures.md), [atoms](../docs/runtime-atoms.md), [generated roots](../docs/generated-roots.md) |
| Runtime representations | [Containers](../docs/container-matching.md), [integers](../docs/integer-matching.md), [floats](../docs/float-matching.md), [maps](../docs/map-matching.md), [bits](../docs/bitstring-matching.md), [records](../docs/record-matching.md) |
| Owned fixtures and reproducible regeneration | [Step 15a](../docs/patternmatch-step15a-validation.md), [fixture instructions](../tests/fixtures/patternmatch/generated/README.md) |
| Final scope, provenance and all test identities | [Step 20 validation](../docs/patternmatch-step20-validation.md), [final evidence](../docs/patternmatch-step20-evidence.json) |

The last reviewed maint-29 pin is `21776803ecd11f5fa948732c0ec66b8f325dedfc`,
unchanged on the 2026-10-03 fetch; installed oracle OTP 29.1.1 / ERTS 17.1.
Historical final validation used LLVM/SDK 23.1.2, Lizard 1.24.0 and clang-tidy
22.1.8: 124/124 Windows x64 CTests, zero skips, 167.43 seconds and all 258
production quality units. Nineteen owned corpora hold 67,634 native expected
values/error reasons plus 106 separate semantic rows. Both drivers/four policies
and two executions yield 1,082,144 comparisons. Seed `0x29A07` adds 1,969
closure outcomes at depth 64, width 255 and 128 alternatives. The documented
example prints 42, -7, record, map, binary, list, integer, other.

All 81 old catalog rows are reconciled: 77 work on admitted values; self/0,
node/0,1 and native is_record/1 need missing owners. Identity/function
predicates currently have only negative classifications on admitted terms. Keep
the recorded OTP huge-literal record-guard loader limitation and legacy-import
compiler crash in steps 17/18 separate from successful oracle execution. Native
Linux, Apple Silicon, 32-bit hosts and new compiler/frontend sanitizer coverage
remain open; earlier macOS/runtime-ASan records preserve their original scope.

## Implementation locations

Use [the existing owner map](files.md#backlog--owners). In particular:

- Memory/GC/copy/trace work: `runtime/src/{memory,terms,process}` and shared
  `abi/`.
- Source contexts, records, callable binding and facts: `compiler/src/semantic/`
  and `semantic/types/`; lowering and frame/safepoint integration:
  `compiler/src/codegen/`.
- Process/identity/signals/receive: `runtime/src/{process,scheduler,terms}`;
  atoms/registries/native adapters: `runtime/src/{terms,modules,builtins}`.
- Executable linking: reserved `compiler/src/linking/`; startup: reserved
  `runtime/src/startup/`; create these only during their implementation step.
- Project schema/graphs/cache/watch: `compiler/src/project/`, driver and
  artifacts owners. Debug/specialization live in existing codegen owners;
  profiling may add the reserved runtime profiling owner when selected.
- Extend owning CLI/native/runtime tests and project-owned fixture groups.
  `compiler/src/stage_readers/{preprocessed,abstract,ir}/` remain reservations
  only.

## Backlog coverage index

Every unchecked feature is mapped below; the detailed steps carry actions,
success criteria and tests. F15 has no remaining unchecked items and is covered
by regression workflows. The common gate applies in addition to each row's
tests.

| Backlog owner | New plan steps |
| --- | --- |
| F01 — Production executable startup and linking | [28](#step-28), [29](#step-29), [30](#step-30) |
| F02 — Roots, safepoints and generated-code ABI evolution | [2](#step-2), [3](#step-3), [4](#step-4), [6](#step-6), [8](#step-8), [19](#step-19), [23](#step-23), [24](#step-24), [26](#step-26) |
| F03 — Process heaps and TermFactory construction | [3](#step-3), [4](#step-4), [12](#step-12), [13](#step-13), [15](#step-15), [16](#step-16), [26](#step-26) |
| F04 — Process garbage collection | [3](#step-3), [4](#step-4), [26](#step-26) |
| F05 — Graph copying and process isolation | [5](#step-5), [23](#step-23), [26](#step-26) |
| F06 — Atom storage and atom expressions | [21](#step-21) |
| F07 — Process, port and reference identities | [15](#step-15), [16](#step-16), [20](#step-20), [27](#step-27) |
| F08 — Lists, tuples, maps and strings | [3](#step-3), [4](#step-4), [5](#step-5), [17](#step-17), [26](#step-26) |
| F09 — Binaries and bitstrings | [3](#step-3), [4](#step-4), [5](#step-5), [26](#step-26) |
| F10 — Arbitrary integers and integer arithmetic | [3](#step-3), [4](#step-4), [5](#step-5), [26](#step-26) |
| F11 — Floating-point values and arithmetic | [3](#step-3), [4](#step-4), [5](#step-5), [17](#step-17), [26](#step-26) |
| F12 — Equality, comparisons and term ordering | [12](#step-12), [13](#step-13), [15](#step-15), [16](#step-16) |
| F13 — Pattern matching and bindings | [7](#step-7), [8](#step-8), [9](#step-9), [10](#step-10), [12](#step-12), [13](#step-13), [24](#step-24), [30](#step-30) |
| F14 — Guards | [7](#step-7), [8](#step-8), [9](#step-9), [10](#step-10), [12](#step-12), [13](#step-13), [24](#step-24), [27](#step-27), [30](#step-30) |
| F16 — Expression sequences and control flow | [7](#step-7), [9](#step-9), [10](#step-10), [30](#step-30) |
| F17 — Record expansion and execution | [11](#step-11), [12](#step-12), [27](#step-27), [30](#step-30) |
| F18 — Closures and function values | [13](#step-13), [26](#step-26), [27](#step-27), [30](#step-30) |
| F19 — Dynamic calls | [14](#step-14), [30](#step-30) |
| F20 — Erlang exceptions | [2](#step-2), [8](#step-8), [26](#step-26), [30](#step-30) |
| F21 — Recursion and proper tail calls | [2](#step-2), [6](#step-6), [30](#step-30) |
| F22 — Cooperative process execution | [2](#step-2), [19](#step-19), [20](#step-20), [26](#step-26), [30](#step-30) |
| F23 — Scheduler workers and wakeups | [22](#step-22), [25](#step-25) |
| F24 — Signals and message sending | [23](#step-23) |
| F25 — Selective receive and timeouts | [24](#step-24), [25](#step-25), [26](#step-26), [30](#step-30) |
| F26 — Production builtin functions | [17](#step-17), [20](#step-20), [27](#step-27), [30](#step-30) |
| F27 — Typed/native callables and conversions | [18](#step-18) |
| F28 — Concurrent code-server access | [21](#step-21) |
| F29 — Useful source-driven specialization | [31](#step-31) |
| F30 — Debug information | [32](#step-32) |
| F31 — Profiling | [33](#step-33) |
| F32 — Link-time optimization | [34](#step-34) |
| V01 — Native platform matrix | [35](#step-35), [36](#step-36) |
| V02 — Compiler/frontend sanitizers | [37](#step-37) |
| V03 — Broader OTP compatibility evidence | [1](#step-1), [30](#step-30), [38](#step-38) |
| V04 — Remaining test migration | [1](#step-1), [39](#step-39) |
| D01 — Dynamic modules and code upgrades | [40](#step-40) |
| D02 — Atom collection | [41](#step-41) |
| D03 — Behavior-changing attributes and transforms | [42](#step-42) |
| D04 — Public stage interchange | [43](#step-43) |
| D05 — Intermediate-stage readers | [44](#step-44) |
| D06 — C/FFI interoperability | [45](#step-45) |
| D07 — Project schema and build-workflow extensions | [46](#step-46), [47](#step-47), [48](#step-48), [49](#step-49), [50](#step-50) |

## Expanded implementation steps

<a id="step-1"></a>

### 1. Establish the remaining compatibility and validation baseline

Backlog: V03, V04. Dependencies: the completed baseline.

- [ ] Perform the implementation or scope actions.
  - [ ] Inventory every unchecked backlog item and record its source forms,
    runtime owner, observable failures and dependencies in the capability
    matrix.
  - [ ] At implementation start, refresh official maint-29 through
    docs/otp-reference.md; synchronize checkout, pin, grammar/corpus hashes and
    current documentation, retaining historical revisions.
  - [ ] Select representative pure Erlang project fixtures and required builtin
    families; record licenses, entrypoints, expected output and explicit
    exclusions before enabling new syntax.
  - [ ] Capture a fresh combined baseline and create a per-step evidence ledger
    distinguishing native execution, oracle regeneration, syntax parsing,
    foreign objects and unavailable runners.
  - IMPORTANT: OTP source and copied files from OTP source remain transient and
    never join the ErlangAoT git, if necessary, save observations/oracle
    data/gold master data in ErlangAoT git, but not the license-protected files.
- [ ] Meet the success criteria.
  - [ ] Every remaining feature has an owner and an implementation or
    conditional decision step in this plan.
  - [ ] The existing admitted-domain behavior passes before the new runtime/ABI
    work begins.
- [ ] Run the necessary tests.
  - [ ] Run the current 19 owned corpora and complete combined Debug gate;
    report any baseline drift rather than changing expected results to hide it.
  - [ ] Run opt-in source/catalog/grammar audits against the reviewed pin and
    explicitly regenerate/check only affected goldens.
  - [ ] Verify deliberately stale provenance and unavailable feature diagnostics
    still fail before publication.

<a id="step-2"></a>

### 2. Choose and implement the resumable call and frame foundation

Backlog: F02, F20, F21, F22. Dependencies: [1](#step-1).

- [ ] Perform the implementation or scope actions.
  - [ ] Compare explicit runtime frames/continuations with LLVM coroutine
    support using a small compiled call/yield/resume prototype; document stack,
    allocation, exception and platform tradeoffs.
  - [ ] Specify entry, call, return, tail transfer, yield, resume and exit
    outcomes, including context ownership and the relationship to the existing
    checked error channel.
  - [ ] Implement the chosen bounded frame/continuation storage and minimal
    transition helpers, with slots for live terms, return locations and handler
    state.
  - [ ] Version descriptors/services only where the machine or ownership
    contract changes; coordinate compiler/runtime migration and reject
    incompatible consumers before entry.
- [ ] Meet the success criteria.
  - [ ] One documented protocol supports ordinary calls, future suspension and
    tail transfer without ambiguous ownership.
  - [ ] A saved continuation can be resumed or destroyed exactly once, and
    failed setup restores the caller state.
- [ ] Run the necessary tests.
  - [ ] Compile a two-module prototype through the real lowering and runtime,
    testing return, nested yield/resume and early failure.
  - [ ] Inject frame allocation failure and attempt duplicate resume,
    wrong-context resume and old-ABI registration.
  - [ ] Inspect 32/64-bit layouts and verify LLVM before/after optimization;
    distinguish these checks from native host execution.

<a id="step-3"></a>

### 3. Unify tracing and roots for reclaimable process storage

Backlog: F02, F03, F04, F08, F09, F10, F11. Dependencies: [2](#step-2).

- [ ] Perform the implementation or scope actions.
  - [ ] Inventory host handles, generated slots, error payloads, frames,
    constructors, atom/module pins and shared binary resources; define root
    registration and release for each owner.
  - [ ] Add bounded layout visitors for every currently admitted term and
    explicit trace/destroy hooks for C++ resource-bearing cells.
  - [ ] Choose stable or relocatable handle semantics with the collector design;
    if relocation is selected, define every slot update before permitting
    movement.
  - [ ] Integrate safepoint/root publication with allocation, call boundaries
    and continuation save/restore; specify interfaces for later closure,
    identity and mailbox owners.
- [ ] Meet the success criteria.
  - [ ] Every admitted live reference has a traceable owner, including native
    host/error handles.
  - [ ] Traversal cannot mistake headers, stale words or interior addresses for
    valid terms; no C++ resource object is moved as raw bytes.
- [ ] Run the necessary tests.
  - [ ] Trace deeply nested/shared graphs of every admitted representation
    through host, generated and error roots.
  - [ ] Exercise partial constructors, expired handles, foreign ownership and
    nested frame teardown under allocation faults.
  - [ ] Inspect generated IR to prove roots are published before allocating
    calls and cleared only after owned handoff.

<a id="step-4"></a>

### 4. Implement process garbage collection and allocation retry

Backlog: F02, F03, F04, F08, F09, F10, F11. Dependencies: [3](#step-3).

- [ ] Perform the implementation or scope actions.
  - [ ] Select and document the collector policy, object discovery, live-set
    accounting, collection triggers and treatment of pinned/shared resources.
  - [ ] Implement bounded marking/tracing and reclamation; add forwarding and
    root rewriting only if the selected collector moves objects.
  - [ ] Connect heap pressure to collection and bounded allocation retry,
    preserving transactional construction and exact infrastructure failure
    statuses.
  - [ ] Reclaim unreachable cells/resources and expose testable accounting for
    live, reusable and externally retained storage without changing term
    semantics.
- [ ] Meet the success criteria.
  - [ ] Repeated allocation and collection stabilizes memory for a bounded live
    set while all rooted values remain valid.
  - [ ] Collection failure preserves the live graph and leaves the process
    usable or terminates it with the specified infrastructure outcome.
- [ ] Run the necessary tests.
  - [ ] Run allocation-heavy Erlang kernels and small-heap native consumers
    retaining nested values and error payloads across collections.
  - [ ] Verify last-owner release for large binaries and other C++ resources,
    shared subgraphs, host pins and process teardown.
  - [ ] Inject tracing/workspace/allocation failures at collection boundaries;
    check retry, accounting and absence of leaks/double destruction.

<a id="step-5"></a>

### 5. Implement bounded graph copying and heap isolation

Backlog: F05, F08, F09, F10, F11. Dependencies: [4](#step-4).

- [ ] Perform the implementation or scope actions.
  - [ ] Define source/destination ownership, preservation of internal sharing,
    immutable resource retention and cross-runtime atom translation or explicit
    rejection.
  - [ ] Implement iterative copying with a forwarding map, rooted scratch state,
    destination budgets and rollback; integrate Term::copy_to and heap addition.
  - [ ] Copy integers, floats, tuples, improper lists, maps and bitstrings
    through validated layout visitors; add an extension contract for later
    identity/closure/native-record layouts.
  - [ ] Make source teardown independent of copied values while preventing
    destination terms from retaining borrowed source-heap addresses.
- [ ] Meet the success criteria.
  - [ ] Copied graphs compare correctly and survive collection or destruction of
    either process independently.
  - [ ] Failed copies publish no partial graph and preserve both heaps and
    resource counts.
- [ ] Run the necessary tests.
  - [ ] Copy nested/shared graphs, exact mixed-type map keys, partial-byte
    bitstrings and large binary views between real contexts.
  - [ ] Test same-context, cross-context and cross-runtime policies, including
    expired sources and foreign atoms.
  - [ ] Inject destination exhaustion and partial-copy failures; collect both
    heaps and verify sharing/resource lifetimes.

<a id="step-6"></a>

### 6. Enable recursion and proper tail transfer

Backlog: F21, F02. Dependencies: [2](#step-2), [4](#step-4).

- [ ] Perform the implementation or scope actions.
  - [ ] Replace blanket cycle rejection with strongly connected call components
    and bounded fixed-point inference that widens safely on exhaustion.
  - [ ] Lower local, remote and mutual recursion through the selected frame
    protocol; distinguish tail and non-tail call sites.
  - [ ] Reuse or replace tail frames without retaining dead roots, handlers or
    arguments; bound non-tail stack/frame growth with a defined failure.
  - [ ] Add reduction/safepoint hooks suitable for later scheduling while
    retaining the existing public call and failure behavior.
- [ ] Meet the success criteria.
  - [ ] Tail-recursive execution has bounded native stack and bounded live frame
    usage at arbitrary iteration counts.
  - [ ] Recursive inference terminates conservatively and non-tail limits fail
    cleanly.
- [ ] Run the necessary tests.
  - [ ] Run long local/remote/mutual tail loops and non-tail recursive kernels
    through both drivers and all policies.
  - [ ] Collect during recursive allocation; verify retained arguments, returned
    values, errors and cleanup.
  - [ ] Exhaust inference/frame/work budgets and compare generic fallback or
    specified resource failure with unrestricted execution.

<a id="step-7"></a>

### 7. Lower blocks, case and if with safe branch bindings

Backlog: F13, F14, F16. Dependencies: [6](#step-6).

- [ ] Perform the implementation or scope actions.
  - [ ] Define lexical scopes and exported-variable rules for begin blocks, case
    branches and if alternatives using the pinned lint semantics.
  - [ ] Reuse normalized matching and guard continuations while preserving the
    single evaluation of case input and ordered branch selection.
  - [ ] Implement success joins for values/bindings and the correct no-branch
    exception reasons; keep unsafe reads located at their source.
  - [ ] Extend inference, call discovery, capability checks, root liveness and
    both inspection modes to every branch, including unreachable syntax.
- [ ] Meet the success criteria.
  - [ ] Branch selection, single assignment and visible bindings match the
    reviewed OTP cases.
  - [ ] Only proofs common to all incoming successful paths survive a join;
    checked extraction remains dominated by shape tests.
- [ ] Run the necessary tests.
  - [ ] Compare nested case/if/block kernels with overlapping patterns, repeated
    variables, wrong types and no selected branch against owned OTP goldens.
  - [ ] Test unsafe/unbound reads, illegal guards in unreachable alternatives
    and source locations through includes/macros.
  - [ ] Use side-effect counters or observable calls to prove input evaluation
    order; exercise collection and failed branch cleanup.

<a id="step-8"></a>

### 8. Implement Erlang raising and catch/try/after handlers

Backlog: F20, F13, F14, F02. Dependencies: [2](#step-2), [6](#step-6),
[7](#step-7).

- [ ] Perform the implementation or scope actions.
  - [ ] Define error/exit/throw class, reason, stack information and uncaught
    outcomes; distinguish catchable Erlang exceptions from infrastructure
    failures.
  - [ ] Add rooted handler frames and explicit inspect/consume/restore
    operations to the existing first-error channel.
  - [ ] Lower source raising, catch and try/of/catch/after with pattern/guard
    scopes, evaluation order and exactly-once cleanup.
  - [ ] Propagate exceptions through generated local/remote calls, builtin
    callbacks and recursive frames; specify saved handler state for future
    yields.
- [ ] Meet the success criteria.
  - [ ] Nested handlers observe the correct class/reason and after clauses run
    on every specified path.
  - [ ] Handled failures clear only their own state; unhandled and
    infrastructure failures preserve ownership and unwind safely.
- [ ] Run the necessary tests.
  - [ ] Compile retained/adapted trycatch suite helpers covering nested
    handlers, failing guards, rethrow, after failure and stack observations.
  - [ ] Run cross-module and recursive exceptions with heap-valued reasons
    across collection and independent retry.
  - [ ] Inject native callback/resource failures and verify they neither escape
    as C++ exceptions nor silently become ordinary guard rejection.

<a id="step-9"></a>

### 9. Implement maybe expressions and conditional matching

Backlog: F16, F13, F14. Dependencies: [7](#step-7), [8](#step-8).

- [ ] Perform the implementation or scope actions.
  - [ ] Specify maybe body/else scopes, conditional-match result routing and
    feature-version requirements from the pinned sources.
  - [ ] Reuse body matching with separate ordinary-error and
    conditional-mismatch continuations; preserve RHS evaluation order.
  - [ ] Lower else selection and joins without leaking body-only or failed-match
    bindings.
  - [ ] Extend diagnostics, inference, inspection and root cleanup for nested
    maybe expressions.
- [ ] Meet the success criteria.
  - [ ] Conditional mismatch reaches the correct else/result path while genuine
    exceptions retain ordinary propagation.
  - [ ] Variables are visible only in the scopes permitted by the reference
    semantics.
- [ ] Run the necessary tests.
  - [ ] Compare success, first/late mismatch, absent else, unmatched else and
    nested exceptions with OTP goldens.
  - [ ] Test feature flags, unsafe bindings and pattern/key/size legality in
    both CLI drivers.
  - [ ] Exercise allocating RHS expressions, collection, evaluation counters and
    all optimization policies.

<a id="step-10"></a>

### 10. Implement list, binary and map comprehensions

Backlog: F16, F13, F14. Dependencies: [5](#step-5), [6](#step-6), [7](#step-7),
[8](#step-8).

- [ ] Perform the implementation or scope actions.
  - [ ] Inventory ordinary, strict and zipped generator forms supported by the
    pinned grammar; record per-form behavior and feature flags.
  - [ ] Define generator/filter scopes, mismatch versus error behavior,
    evaluation order and output accumulation rules.
  - [ ] Lower bounded iteration/builders with rooted intermediate state and the
    recursion/safepoint protocol; reuse guard and pattern services.
  - [ ] Enable forms only after their list/binary/map result builders and
    failure cleanup are complete; report any unselected experimental form
    explicitly.
- [ ] Meet the success criteria.
  - [ ] Every selected comprehension form has executable semantics and correct
    generated value/order.
  - [ ] Large outputs and failed generators respect work/heap limits without
    partial result publication or leaked bindings.
- [ ] Run the necessary tests.
  - [ ] Compare nested generators, dependent filters, strict mismatches, unequal
    zipped lengths and repeated map keys with applicable OTP cases.
  - [ ] Test bit sizes/units, improper generator inputs, empty results and
    exceptions in generator/filter/body expressions.
  - [ ] Run long allocating comprehensions with tiny heaps, forced yields once
    available, wrong specs and all policies.

<a id="step-11"></a>

### 11. Complete ordinary record updates and record_info

Backlog: F17. Dependencies: [7](#step-7), [8](#step-8).

- [ ] Perform the implementation or scope actions.
  - [ ] Extend declaration resolution for ordinary record updates and
    record_info forms, preserving source-order field validation and default
    rules.
  - [ ] Lower updates with one evaluation of the record expression, checked
    tag/arity and rooted source/field temporaries.
  - [ ] Implement record_info only for its legal compile-time forms; preserve
    restrictions in patterns and guards.
  - [ ] Integrate field/type analysis and owned badrecord behavior without
    adding a second ordinary-record representation.
- [ ] Meet the success criteria.
  - [ ] Ordinary record construction, access, matching, updates and
    introspection agree on one declaration layout.
  - [ ] Invalid names/forms diagnose and runtime update mismatch preserves the
    offending value.
- [ ] Run the necessary tests.
  - [ ] Compare record suite update/introspection helpers, reordered fields,
    defaults, nested updates and wrong tags/arities against OTP.
  - [ ] Verify update evaluation order and single evaluation through observable
    calls and exceptions.
  - [ ] Test include-origin diagnostics, illegal guard/pattern updates,
    collection and allocation-failure rollback.

<a id="step-12"></a>

### 12. Implement native, qualified and inferred record forms

Backlog: F17, F03, F12, F13, F14. Dependencies: [4](#step-4), [5](#step-5),
[11](#step-11).

- [ ] Perform the implementation or scope actions.
  - [ ] Audit each remaining record category against the current
    grammar/lint/runtime evidence; document declaration identity, field
    resolution and cross-module visibility.
  - [ ] Define the required native representation and owned declaration/type
    identity, including construction, access, update, comparison and printing
    boundaries.
  - [ ] Add trace/copy/destroy support before source admission; extend
    matching/binding/inference for qualified and inferred forms with located
    diagnostics.
  - [ ] Implement native is_record/1 and any supported identity-dependent record
    tests only when actual native values are constructible.
- [ ] Meet the success criteria.
  - [ ] Every admitted record category has complete ownership and observable
    semantics distinct where the reference requires it.
  - [ ] Ordinary tuples cannot masquerade as native records, and invalid
    declarations remain semantic errors.
- [ ] Run the necessary tests.
  - [ ] Regenerate representative native/qualified/inferred record goldens with
    a working matching oracle; record any oracle loader limitation separately.
  - [ ] Test cross-module declarations, hidden/missing fields, conflicting
    identities, matching, comparison and guard classification.
  - [ ] Copy/collect native records, retain them across module-handle release
    and inject construction/registration failures.

<a id="step-13"></a>

### 13. Implement closures and function values

Backlog: F18, F03, F12, F13, F14. Dependencies: [4](#step-4), [5](#step-5),
[6](#step-6), [8](#step-8).

- [ ] Perform the implementation or scope actions.
  - [ ] Define local/external/named fun identity, arity, captured environments
    and retained code ownership.
  - [ ] Implement rooted closure construction and trace/copy/destroy support,
    including shared captures and any recursive self-reference strategy.
  - [ ] Lower fun clauses, captures, invocation and named recursion using
    existing clause/guard/frame machinery.
  - [ ] Extend equality/order, function predicates, inference and error paths
    without treating declared types as runtime proof.
- [ ] Meet the success criteria.
  - [ ] A closure remains callable after its creator returns and after its
    captured graph is collected or copied.
  - [ ] Arity, failed clause selection and code lifetime follow the defined
    callable contract.
- [ ] Run the necessary tests.
  - [ ] Compare capturing/noncapturing/named/remote fun cases, shadowing, nested
    fun clauses and function predicates with OTP.
  - [ ] Invoke copied closures in another process context and retain them across
    registry-handle release.
  - [ ] Test wrong arity, bad captures, recursive closures, allocation failure,
    collection and thrown exceptions.

<a id="step-14"></a>

### 14. Implement dynamic function and module calls

Backlog: F19. Dependencies: [8](#step-8), [13](#step-13).

- [ ] Perform the implementation or scope actions.
  - [ ] Define supported dynamic fun and module/function call forms, argument
    order, arity checks and badfun/badarity/undef-style outcomes from reference
    evidence.
  - [ ] Resolve dynamic module/function names through pinned runtime code
    handles and owned atom identities.
  - [ ] Lower all operands in source order with roots and dispatch through the
    checked frame/call protocol.
  - [ ] Keep lookup failures, callable failures and infrastructure statuses
    distinguishable and support tail-position dynamic transfer.
- [ ] Meet the success criteria.
  - [ ] Supported dynamic targets behave like their equivalent static calls and
    retain code through invocation.
  - [ ] Bad targets and missing exports produce the specified Erlang outcomes
    without partial caller execution.
- [ ] Run the necessary tests.
  - [ ] Compare variable fun/module/function calls, argument side effects,
    private/missing exports and wrong arities against OTP goldens.
  - [ ] Run dynamic tail loops and nested exceptions through both CLI drivers
    and all policies.
  - [ ] Collect between operand evaluation and dispatch; test stale/foreign
    handles and failed lookup followed by successful retry.

<a id="step-15"></a>

### 15. Implement process and reference identities

Backlog: F07, F03, F12. Dependencies: [3](#step-3), [5](#step-5).

- [ ] Perform the implementation or scope actions.
  - [ ] Define runtime/node identity, uniqueness, reuse protection and the
    distinction between a valid identity value and a currently live process.
  - [ ] Connect pid construction/lookup to real process-context lifecycle
    ownership and reference construction to a checked uniqueness owner.
  - [ ] Add host-term admission, equality/order, printing and copy rules,
    including explicit cross-runtime policy.
  - [ ] Register tracing/lifetime behavior and reject forged words before
    dereferencing identity storage.
- [ ] Meet the success criteria.
  - [ ] Identity values preserve their specified equality after process exit
    without accidentally naming a reused process.
  - [ ] Only owned, validated identities enter the runtime; routing checks
    liveness separately.
- [ ] Run the necessary tests.
  - [ ] Exercise create/destroy/recreate cycles, retained dead-process
    identities and reference uniqueness under bounded stress.
  - [ ] Compare is_pid/is_reference, equality/order and term-copy behavior with
    OTP where expressible.
  - [ ] Test forged/stale/foreign encodings, counter/resource exhaustion and
    rollback of partially created contexts.

<a id="step-16"></a>

### 16. Provide a real lifecycle owner for port identities

Backlog: F07, F03, F12. Dependencies: [15](#step-15).

- [ ] Perform the implementation or scope actions.
  - [ ] Choose a concrete minimal runtime-managed port resource and define
    registration, close, ownership and retained identity semantics.
  - [ ] Implement validated port identity construction/lookup tied to that
    owner; separate port identity support from additional external I/O
    operations.
  - [ ] Add comparison, predicates, copying and cleanup hooks consistent with
    process/reference identities.
  - [ ] Keep unimplemented port I/O services explicitly unavailable and document
    which producer can create a valid port.
- [ ] Meet the success criteria.
  - [ ] Positive port classification is demonstrated using a real owned resource
    rather than a forged tagged word.
  - [ ] Close, retained identity and failed registration release resources
    exactly once.
- [ ] Run the necessary tests.
  - [ ] Use the concrete port producer to test open/register/close and
    positive/negative predicates.
  - [ ] Exercise copied/retained identities, expired resources, foreign
    ownership and duplicate close.
  - [ ] Inject owner/registration allocation failure and compare applicable
    identity/order observations with OTP.

<a id="step-17"></a>

### 17. Implement production builtin registration for selected pure families

Backlog: F26, F08, F11. Dependencies: [8](#step-8), [11](#step-11),
[14](#step-14), [15](#step-15).

- [ ] Perform the implementation or scope actions.
  - [ ] Use the project inventory from step 1 to select concrete list,
    conversion, numeric, map/binary and introspection families; record exact
    name/arity and semantics.
  - [ ] Implement/register generic bridge wrappers using existing value
    services, with checked arguments, rooted outputs and typed Erlang failures.
  - [ ] Add missing algorithms in their term owners, including selected list
    operations, with bounded traversal and explicit allocation behavior.
  - [ ] Keep compiler guard authorization separate from runtime registration and
    leave process/port/distribution families tied to their later owners.
- [ ] Meet the success criteria.
  - [ ] Every selected builtin works through the production registry and
    ordinary compiled source calls.
  - [ ] Compiler-authorized and generic paths agree where they implement the
    same operation; no arbitrary registered function becomes guard-legal.
- [ ] Run the necessary tests.
  - [ ] Compare valid, wrong-type, boundary, improper-list and allocation-heavy
    cases with owned OTP goldens.
  - [ ] Test qualified/unqualified resolution, missing versus unavailable
    signatures and calls from handlers/closures.
  - [ ] Run generic/compiled equivalence, partial-output rejection, resource
    failures and retry under all policies.

<a id="step-18"></a>

### 18. Implement the selected typed native-callable adapters

Backlog: F27. Dependencies: [13](#step-13), [14](#step-14), [17](#step-17).

- [ ] Perform the implementation or scope actions.
  - [ ] Select concrete internal C++ consumers and supported argument/result
    types; document conversion, callback and lifetime rules.
  - [ ] Implement only the required checked adapters, preserving a generic
    Term-call fallback and retained code/module handles.
  - [ ] Keep STL objects and C++ exceptions behind the internal native boundary;
    root borrowed/converted terms throughout callbacks.
  - [ ] Replace or retain unverified typed sketches according to executable
    coverage and document unsupported conversions.
- [ ] Meet the success criteria.
  - [ ] Selected typed calls are observably equivalent to the generic path and
    cannot outlive their owner accidentally.
  - [ ] Conversion/callback failure returns a defined checked outcome without
    publishing partial output.
- [ ] Run the necessary tests.
  - [ ] Run independent C++23 consumers through real generated calls and the
    runtime registry.
  - [ ] Test wrong types/ranges, expired handles, nested callbacks, throwing
    callbacks and generic fallback.
  - [ ] Force collection/allocation failure during conversion and verify
    retained values and same-context recovery.

<a id="step-19"></a>

### 19. Run resumable isolated processes on a cooperative executor

Backlog: F22, F02. Dependencies: [6](#step-6), [8](#step-8), [15](#step-15),
[17](#step-17).

- [ ] Perform the implementation or scope actions.
  - [ ] Connect the selected frame protocol to runtime
    create/start/resume/finish operations and real process identities.
  - [ ] Implement reduction grants and bounded scheduling opportunities for
    recursive calls, allocating loops and long runtime services.
  - [ ] Save rooted continuations and handler state on yield; enforce exclusive
    ownership of a process during execution.
  - [ ] Define normal return, uncaught exception, explicit exit and cancellation
    cleanup before adding worker concurrency.
- [ ] Meet the success criteria.
  - [ ] Generated code can yield and resume with stable values/handlers and
    isolated heaps.
  - [ ] A CPU-bound process cannot indefinitely prevent another runnable process
    from progressing.
- [ ] Run the necessary tests.
  - [ ] Run multiple compiled arithmetic/allocation loops on one cooperative
    executor with deterministic grants.
  - [ ] Yield inside nested calls, handlers and closures, collect suspended
    roots, resume and compare final results.
  - [ ] Inject startup/resume failures and cancellation at transition
    boundaries; verify one completion and complete root/frame cleanup.

<a id="step-20"></a>

### 20. Implement spawn, exit, links and monitors

Backlog: F22, F07, F26. Dependencies: [19](#step-19).

- [ ] Perform the implementation or scope actions.
  - [ ] Define the selected spawn/exit/link/monitor signatures and their process
    state transitions from pinned OTP evidence.
  - [ ] Create children with isolated copied arguments/captures; implement
    completion/exit reasons and owned link/monitor records.
  - [ ] Route lifecycle notifications through a common ordered signal envelope
    that step 23 also uses for messages; define trap-exit and demonitor cleanup.
  - [ ] Register only implemented process BIFs and connect uncaught exceptions,
    runtime shutdown and retained dead identities.
- [ ] Meet the success criteria.
  - [ ] Selected lifecycle operations have deterministic ownership and specified
    notification/cleanup behavior.
  - [ ] Failure during spawn or relationship registration leaves no
    half-published child, link or monitor.
- [ ] Run the necessary tests.
  - [ ] Compare parent/child success, exit, trap-exit and monitor/down traces
    with OTP using a native observer of real lifecycle signals; add source
    receive consumption in step 24.
  - [ ] Test link/monitor teardown races in the serial executor, dead recipients
    and repeated demonitor.
  - [ ] Inject child-copy/registration failures and verify heap isolation,
    reason ownership and shutdown.

<a id="step-21"></a>

### 21. Synchronize atoms and code-server publication

Backlog: F06, F28. Dependencies: [13](#step-13), [15](#step-15),
[17](#step-17), [19](#step-19).

- [ ] Perform the implementation or scope actions.
  - [ ] Define lock/ownership ordering for atom interning, module publication,
    lookup, code pins, context access and runtime shutdown.
  - [ ] Implement concurrent atom lookup/intern and transactional
    descriptor/export/binding publication.
  - [ ] Retain immutable code images and atom bindings through active calls,
    resolved handles and closure destruction.
  - [ ] Add explicit shutdown admission rules and rollback for duplicate or
    failed registrations before worker threads are enabled.
- [ ] Meet the success criteria.
  - [ ] Concurrent readers observe either a complete published module or the
    documented absence.
  - [ ] Atoms and code remain valid for every retained reader while shutdown
    prevents new unsafe work.
- [ ] Run the necessary tests.
  - [ ] Stress duplicate atom/module registration and simultaneous
    lookup/call/closure retention using real threads.
  - [ ] Force allocation failures at publication boundaries and verify no
    partially visible bindings or lost code pins.
  - [ ] Race permitted registration/lookup operations with shutdown; run
    available race diagnostics and retain unavailable tooling as a gap.

<a id="step-22"></a>

### 22. Implement scheduler workers, queues and wakeups

Backlog: F23. Dependencies: [19](#step-19), [20](#step-20), [21](#step-21).

- [ ] Perform the implementation or scope actions.
  - [ ] Specify worker/run-queue ownership, ready/running/waiting/suspended
    states, reduction grants and fairness policy.
  - [ ] Implement worker startup, queue admission, dispatch and load balancing
    with exactly one active owner per process.
  - [ ] Implement wakeup and signal-service paths for waiting or explicitly
    suspended processes without clearing explicit suspension.
  - [ ] Coordinate worker stop/join, in-flight services, runtime shutdown and
    process cancellation with the synchronization rules.
- [ ] Meet the success criteria.
  - [ ] Runnable processes progress under contention and no process executes
    simultaneously on two workers.
  - [ ] Wakeups are not lost, explicit suspension is preserved and shutdown
    joins every worker safely.
- [ ] Run the necessary tests.
  - [ ] Run CPU-bound and frequently yielding compiled processes across one and
    multiple workers with progress assertions.
  - [ ] Use barriers to test enqueue/park/wakeup/shutdown races and waiting
    versus explicitly suspended signal service.
  - [ ] Stress worker/context allocation failures, repeated start/stop and owner
    migration with collection and retained results.

<a id="step-23"></a>

### 23. Implement ordered signals and message sending

Backlog: F24, F05, F02. Dependencies: [5](#step-5), [20](#step-20),
[22](#step-22).

- [ ] Perform the implementation or scope actions.
  - [ ] Define signal envelopes, recipient validation, acceptance versus
    handling, payload roots and ordering across message/lifecycle signal kinds.
  - [ ] Enqueue every message, including self-send, into the signal inbox; avoid
    direct sender-side mailbox insertion.
  - [ ] Perform bounded owner-side signal handling and destination graph copying
    before mailbox insertion, retaining in-transit roots and shared resources.
  - [ ] Lower send and register selected send services with precise invalid/dead
    recipient, resource and delivery outcomes.
- [ ] Meet the success criteria.
  - [ ] Per-sender signal order and self-send semantics are preserved across
    workers and process states.
  - [ ] Mailbox values own their graphs independently and rejected/failed
    handling releases all in-transit resources.
- [ ] Run the necessary tests.
  - [ ] Compare ordered multi-sender/self-send traces with OTP using barriers
    and a native consumer of the real mailbox; step 24 adds source receive
    workflows.
  - [ ] Send closures, native records, large binary views and nested maps;
    collect or terminate the sender before handling.
  - [ ] Test waiting/suspended/dead recipients, copy exhaustion, shutdown during
    delivery and bounded handling fairness.

<a id="step-24"></a>

### 24. Implement selective receive matching and retained cursors

Backlog: F25, F13, F14, F02. Dependencies: [7](#step-7), [8](#step-8),
[23](#step-23).

- [ ] Perform the implementation or scope actions.
  - [ ] Extend binding/guard analysis to receive clauses using the shared
    normalized matcher and located scope diagnostics.
  - [ ] Implement rooted mailbox cursor/candidate ownership and ordered clause
    evaluation without publishing failed candidate bindings.
  - [ ] Remove only the selected message and retain unmatched message order and
    saved scan position.
  - [ ] Integrate receive body execution, nested receive and resource/exception
    cleanup with process frames and reduction accounting.
- [ ] Meet the success criteria.
  - [ ] Receive selects the first eligible message/clause under the specified
    scan order and preserves all unmatched messages.
  - [ ] Guard semantic failures continue scanning while infrastructure failures
    follow the process failure contract.
- [ ] Run the necessary tests.
  - [ ] Compare overlapping clauses, unmatched prefixes, nested receive and
    repeated scans against owned OTP outcomes.
  - [ ] Collect and yield during scans of heap-valued messages; verify
    cursor/candidate roots and failed-binding isolation.
  - [ ] Inject matching/service faults and process exit during selection; verify
    exactly one removal and complete cleanup.

<a id="step-25"></a>

### 25. Implement receive waiting and timeout arbitration

Backlog: F25, F23. Dependencies: [24](#step-24).

- [ ] Perform the implementation or scope actions.
  - [ ] Define the mailbox-tail/arrival handshake, wait registration, wakeup
    generation and a monotonic timer contract.
  - [ ] Lower after expressions with specified evaluation timing and validate
    zero, finite, infinite and invalid timeout values.
  - [ ] Implement suspend/resume, timer cancellation and arrival-versus-timeout
    arbitration with one winning continuation.
  - [ ] Preserve explicit suspension and retained cursors across signals, timer
    events and shutdown.
- [ ] Meet the success criteria.
  - [ ] A matching arrival cannot be lost between finishing a scan and parking.
  - [ ] Each receive resumes once with either a selected message or the timeout
    path; stale timers cannot resume a later receive.
- [ ] Run the necessary tests.
  - [ ] Use controlled timer/arrival barriers to cover both race outcomes,
    zero-time polling, infinity and repeated waits.
  - [ ] Compare observable receive-after programs with OTP without relying on
    fragile exact timing thresholds.
  - [ ] Test timer allocation failure, cancellation, explicit suspension and
    process/runtime teardown during a wait.

<a id="step-26"></a>

### 26. Close GC, continuation and mailbox lifetime integration

Backlog: F02, F03, F04, F05, F08, F09, F10, F11, F18, F20, F22, F25.
Dependencies: [12](#step-12), [13](#step-13), [16](#step-16), [25](#step-25).

- [ ] Perform the implementation or scope actions.
  - [ ] Reconcile the root/trace/copy inventory with all new layouts,
    runnable/suspended frames, handlers, inbox messages and receive cursors.
  - [ ] Exercise collection and owner migration at every permitted safepoint,
    fixing missing roots and stale borrows in their actual owners.
  - [ ] Complete resource accounting for shared binaries, copied closures,
    native records, identities and retained exception payloads.
  - [ ] Document bounded work and cleanup guarantees across spawn, send,
    receive, cancellation and shutdown.
- [ ] Meet the success criteria.
  - [ ] Every admitted live graph survives collection across execution and
    waiting states, and unreachable resources are reclaimed.
  - [ ] Stress cycles converge to the expected retained live set with no
    cross-process heap aliasing.
- [ ] Run the necessary tests.
  - [ ] Run long small-heap process rings exchanging nested values while
    yielding, throwing, receiving and timing out.
  - [ ] Retain host handles/errors while creator processes exit; verify the
    documented access and lifetime behavior.
  - [ ] Sweep allocation/root/copy/timer failures and run supported memory/race
    diagnostics with no blanket suppression.

<a id="step-27"></a>

### 27. Complete identity-dependent guard and service availability

Backlog: F14, F26, F07, F17, F18. Dependencies: [12](#step-12), [13](#step-13),
[16](#step-16), [20](#step-20), [26](#step-26).

- [ ] Perform the implementation or scope actions.
  - [ ] Re-audit the exact pinned guard catalog and connect
    self/node/native-record services to their implemented owners.
  - [ ] Define the supported local node identity and node/1 behavior; keep
    distribution explicitly separate if no distribution owner exists.
  - [ ] Enable positive pid/port/reference/function/native-record predicates
    only for constructible owned values.
  - [ ] Update semantic-versus-capability diagnostics, qualified/legacy
    resolution, signature mappings and owned fixture manifests.
- [ ] Meet the success criteria.
  - [ ] Every enabled catalog row has valid, invalid and boundary evidence using
    real admitted values.
  - [ ] Previously blocked signatures are either implemented with their owners
    or remain explicitly recorded as a concrete unresolved requirement.
- [ ] Run the necessary tests.
  - [ ] Compare positive/negative predicates and self/node/record queries with
    OTP across local/remote and guard/body contexts.
  - [ ] Test illegal arities, imports/shadowing, unreachable operands and
    forged/foreign identity rejection.
  - [ ] Run all admitted-domain corpora and fault policies, including guard
    rejection versus infrastructure termination.

<a id="step-28"></a>

### 28. Generate production startup and entrypoint execution

Backlog: F01. Dependencies: [17](#step-17), [20](#step-20), [26](#step-26),
[27](#step-27).

- [ ] Perform the implementation or scope actions.
  - [ ] Define module/function entry selection, CLI argument encoding, runtime
    options, exit codes and uncaught-error reporting.
  - [ ] Generate startup that creates the runtime, registers the complete module
    set, starts the root process and drives execution to the selected shutdown
    condition.
  - [ ] Implement rollback for partial registration/startup and orderly
    worker/process/resource shutdown.
  - [ ] Add a reusable runtime startup owner while preserving explicit internal
    C++ consumers.
- [ ] Meet the success criteria.
  - [ ] A generated startup executes a compiled Erlang entrypoint and reports
    completion/failure without a handwritten application harness.
  - [ ] Startup failures and root-process failures return documented statuses
    and release all created resources.
- [ ] Run the necessary tests.
  - [ ] Link generated startup with real objects and test arguments, return
    values, output and exit status.
  - [ ] Exercise missing/invalid entries, duplicate modules, incompatible
    descriptors and registration/startup allocation failures.
  - [ ] Run entrypoints that spawn, send/receive, collect and fail; verify
    shutdown and stderr diagnostics.

<a id="step-29"></a>

### 29. Drive native executable linking from both public drivers

Backlog: F01. Dependencies: [28](#step-28).

- [ ] Perform the implementation or scope actions.
  - [ ] Implement target-runtime selection and Clang/linker invocation with
    explicit architecture, build configuration and ABI compatibility checks.
  - [ ] Connect positional/project --output to generated startup, module objects
    and exactly one matching runtime dependency.
  - [ ] Stage executable/link intermediates and publish only after successful
    linking; preserve existing artifact collision and failure behavior.
  - [ ] Report tool invocation/link diagnostics with source/target context and
    document supported host/cross-link combinations.
- [ ] Meet the success criteria.
  - [ ] Both drivers produce directly runnable native executables with
    reproducible inputs and correct runtime linkage.
  - [ ] Missing tools/runtime or failed linking leaves no partially published
    executable or corrupted prior output.
- [ ] Run the necessary tests.
  - [ ] Build/run single- and multi-module programs through both drivers at all
    four policies with paths containing spaces and Unicode.
  - [ ] Test wrong runtime width/ABI, missing symbols/libraries, output
    conflicts, interrupted/failed publication and successful retry.
  - [ ] Inspect dependencies to verify generated executables do not accidentally
    acquire compiler/LLVM libraries.

<a id="step-30"></a>

### 30. Validate representative pure Erlang projects end to end

Backlog: F01, F13, F14, F16, F17, F18, F19, F20, F21, F22, F25, F26, V03.
Dependencies: [29](#step-29).

- [ ] Perform the implementation or scope actions.
  - [ ] Finalize the small real-project corpus chosen in step 1, retaining
    project-owned licensed sources and explicit external dependency boundaries.
  - [ ] Compile projects through their public manifests into executables;
    resolve in-scope missing standard-library/BIF behavior in existing owners.
  - [ ] Compare outputs, errors, process protocols and termination with recorded
    OTP executions, preserving meaningful original source behavior.
  - [ ] Publish a supported-project/source-feature matrix and exact exclusions;
    do not describe selected-project compatibility as universal Erlang support.
- [ ] Meet the success criteria.
  - [ ] Selected pure Erlang projects build and run through the production
    workflow with reproducible observable results.
  - [ ] Each incompatibility is fixed or recorded as a specific remaining owner
    rather than hidden by fixture weakening.
- [ ] Run the necessary tests.
  - [ ] Run multi-module libraries/tools plus a concurrent message-driven
    example in both drivers and all policies.
  - [ ] Exercise fresh builds, repeated builds, failed source batches,
    executable replacement and post-failure recovery.
  - [ ] Run small-heap and long-lived workloads, wrong specs and ordinary error
    cases against owned expected results.

<a id="step-31"></a>

### 31. Make specialization remove proven source checks

Backlog: F29. Dependencies: [30](#step-30).

- [ ] Perform the implementation or scope actions.
  - [ ] Measure actual generic IR and identify source call profiles that permit
    a useful check removal or representation-specific operation.
  - [ ] Extend implementation inference/proof consumers for selected operations
    without allowing specs to authorize access.
  - [ ] Retain generic ABI fallback, existing variant/work/growth ceilings and
    whole-draft rollback; verify fresh IR on both paths.
  - [ ] Record compile cost, code size, remaining checks and execution
    measurements for annotated/unannotated source.
- [ ] Meet the success criteria.
  - [ ] At least one real supported source case removes a demonstrably redundant
    check while preserving adversarial behavior.
  - [ ] Budget exhaustion and unproven profiles select a valid generic path
    without partial publication.
- [ ] Run the necessary tests.
  - [ ] Compare specialized/generic executables at O0/O2 with correct, missing
    and intentionally wrong specs.
  - [ ] Inspect dominance/check removal and verify LLVM before/after transforms;
    exercise hit/miss paths and all existing caps.
  - [ ] Record descriptive timings and size/cost measurements without unstable
    performance pass thresholds.

<a id="step-32"></a>

### 32. Emit and validate source-level debug information

Backlog: F30. Dependencies: [29](#step-29).

- [ ] Perform the implementation or scope actions.
  - [ ] Select initial Windows/Unix debugger formats and define source mappings
    through macros, includes and generated helper blocks.
  - [ ] Emit compile units, functions, line scopes and supported variable/term
    locations through object and executable linking.
  - [ ] Map recursive/continuation frames to inspectable Erlang frames where
    supported; document optimized and suspended-state limitations.
  - [ ] Keep debug metadata optional and bounded, with stable diagnostics for
    unsupported inspection requests.
- [ ] Meet the success criteria.
  - [ ] Documented debugger workflows hit Erlang source breakpoints and show
    correct source/frame identity.
  - [ ] Supported values are inspectable and unavailable optimized/suspended
    values are represented honestly.
- [ ] Run the necessary tests.
  - [ ] Automate available debugger batch sessions for nested calls,
    includes/macros, exceptions and recursive frames.
  - [ ] Inspect emitted debug sections and verify executable results with debug
    information enabled/disabled.
  - [ ] Repeat on supported native hosts and record missing debugger/platform
    coverage explicitly.

<a id="step-33"></a>

### 33. Add opt-in runtime and generated-code profiling

Backlog: F31. Dependencies: [26](#step-26), [29](#step-29).

- [ ] Perform the implementation or scope actions.
  - [ ] Select concrete counters/sampling events for functions, allocations,
    reductions and scheduling; define ownership and attribution.
  - [ ] Implement bounded collection/export with module/function identity and
    source linkage where available.
  - [ ] Keep instrumentation disabled by default and define overflow, concurrent
    collection and shutdown behavior.
  - [ ] Document data interpretation, overhead and limitations, including
    shared/runtime work attribution.
- [ ] Meet the success criteria.
  - [ ] Profiles attribute known workloads to the correct functions/process
    activities with bounded resource use.
  - [ ] Disabled profiling preserves normal behavior and avoids collection
    state.
- [ ] Run the necessary tests.
  - [ ] Run known call/allocation/scheduling workloads and verify expected
    attribution and counter invariants.
  - [ ] Test overflow, exporter failure, concurrent process exit and final
    flush/teardown.
  - [ ] Compare enabled/disabled outputs and report measured overhead without
    noisy performance assertions.

<a id="step-34"></a>

### 34. Integrate selected link-time optimization modes

Backlog: F32. Dependencies: [29](#step-29), [31](#step-31).

- [ ] Perform the implementation or scope actions.
  - [ ] Choose supported LTO modes/toolchains and define compatibility with
    target runtime, debug information and symbol retention.
  - [ ] Wire bitcode/link inputs and cache/temp ownership into the public
    linking workflow.
  - [ ] Preserve startup, descriptors, exports, runtime references, code pins
    and failure transport across whole-program optimization.
  - [ ] Document supported combinations and retain ordinary-link fallback and
    failed-link cleanup.
- [ ] Meet the success criteria.
  - [ ] Selected LTO builds execute identically to ordinary linked builds and
    keep required registrations/exports reachable.
  - [ ] Unsupported or mismatched inputs fail before publishing an executable.
- [ ] Run the necessary tests.
  - [ ] Compare LTO/non-LTO results for cross-module calls, closures, dynamic
    lookup, exceptions and concurrent processes.
  - [ ] Inspect symbols/dependencies and test missing runtime, bitcode version
    mismatch and interrupted linking.
  - [ ] Record binary size, compile/link cost and native platform/tool versions.

<a id="step-35"></a>

### 35. Close native Windows and Linux x86-family coverage

Backlog: V01. Dependencies: [30](#step-30), [34](#step-34).

- [ ] Perform the implementation or scope actions.
  - [ ] Prepare matching compiler/SDK/runtime toolchains and runners for Windows
    x86/x64 and Linux x86/x64.
  - [ ] Configure fresh native compiler/runtime/testing builds with warnings and
    quality checks enabled.
  - [ ] Run generated executable, GC/process/message, ABI, failure and lifecycle
    workflows at all four policies.
  - [ ] Fix platform failures in their owners and publish tool versions and
    pass/fail/skip/unavailable results per runner.
- [ ] Meet the success criteria.
  - [ ] Each claimed x86-family target has actual native execution evidence for
    the current implementation.
  - [ ] A missing runner keeps its matrix cell open; inspected foreign objects
    do not satisfy it.
- [ ] Run the necessary tests.
  - [ ] Run full CTest/quality and focused 32-bit payload/layout/ABI boundary
    regressions on each available host.
  - [ ] Run the representative project corpus and documented
    executable/debug/LTO workflows where supported.
  - [ ] Exercise target-runtime mismatch, alignment, native calling conventions
    and clean startup/shutdown.

<a id="step-36"></a>

### 36. Close native Linux ARM and Apple Silicon coverage

Backlog: V01. Dependencies: [30](#step-30), [34](#step-34).

- [ ] Perform the implementation or scope actions.
  - [ ] Prepare matching Linux ARM/AArch64 and macOS Apple Silicon native
    runners/toolchains.
  - [ ] Audit target-derived alignment, segment endianness, calling convention,
    atomics and shared resource destruction.
  - [ ] Run fresh combined builds, quality and native
    compiler/runtime/executable workflows.
  - [ ] Publish current per-host evidence while preserving earlier macOS
    skeleton results as historical records.
- [ ] Meet the success criteria.
  - [ ] Each claimed ARM-family target executes the current project corpus and
    runtime integrations natively.
  - [ ] Architecture-specific failures are resolved without host-width
    assumptions or weakened checks.
- [ ] Run the necessary tests.
  - [ ] Run full CTest/quality and all four native policy combinations for
    representative projects.
  - [ ] Exercise unaligned bit segments, numeric boundaries, GC roots,
    concurrent queues and ABI rejection.
  - [ ] Record supported debugger/LTO results separately and list every
    unavailable runner/configuration.

<a id="step-37"></a>

### 37. Close sanitizer and concurrency diagnostic gaps

Backlog: V02. Dependencies: [26](#step-26), [30](#step-30).

- [ ] Perform the implementation or scope actions.
  - [ ] Select compatible SDK/CRT/allocator configurations that resolve the
    recorded annotation and allocator ABI conflicts.
  - [ ] Build instrumented compiler/frontend/runtime configurations for
    available ASan, UBSan and LeakSanitizer; add thread/race diagnostics where
    supported.
  - [ ] Define which SDK, runtime, generated code and native adapters are
    actually instrumented.
  - [ ] Fix findings and retain exact commands, tool versions, scope and
    unsupported configurations without blanket suppressions.
- [ ] Meet the success criteria.
  - [ ] Required available sanitizer configurations pass meaningful full
    workflows and lifetime/failure stress.
  - [ ] Unavailable configurations remain explicit evidence gaps and are not
    counted as passed coverage.
- [ ] Run the necessary tests.
  - [ ] Run malformed/deep frontend input, OOM rollback,
    GC/copy/closure/exception and process/mailbox workloads under
    instrumentation.
  - [ ] Exercise concurrent registration, wakeup/timeout/shutdown races with
    available race diagnostics.
  - [ ] Verify leak-free teardown of retained resources and distinguish
    uninstrumented SDK/generated-code limits.

<a id="step-38"></a>

### 38. Broaden pinned OTP suite and differential evidence

Backlog: V03. Dependencies: [30](#step-30).

- [ ] Perform the implementation or scope actions.
  - [ ] Refresh and synchronize official maint-29, then build a matching OTP
    reference in an explicit audit environment.
  - [ ] Select applicable upstream Common Test suites and required hooks; record
    upstream-only execution separately from compiled ErlangAoT kernels.
  - [ ] Add project-owned goldens for enabled language/process services with
    exact source/function/version/hash/adaptation provenance.
  - [ ] Publish suite outcomes, exclusions, upstream failures and compiler
    compatibility gaps without silently changing expected behavior.
- [ ] Meet the success criteria.
  - [ ] Compatibility claims can be traced to exact executed sources and
    matching oracle outcomes.
  - [ ] Routine builds/tests remain independent of OTP and all new goldens
    reproduce through explicit regeneration.
- [ ] Run the necessary tests.
  - [ ] Run the selected upstream suites on the matching reference and all
    retained/adapted kernels through the public compiler/executables.
  - [ ] Verify stale-hash and wrong-version rejection, semantic acceptance
    versus syntax acceptance and deterministic regeneration.
  - [ ] Compare stable values/error classes/reasons and process protocol
    outcomes across drivers/policies and available native hosts.

<a id="step-39"></a>

### 39. Finish behavioral test migration without losing invariants

Backlog: V04. Dependencies: [30](#step-30), [38](#step-38).

- [ ] Perform the implementation or scope actions.
  - [ ] Audit every remaining adapter/synthetic success test in
    docs/test-migration.md against current CLI/executable coverage.
  - [ ] Add missing equivalent real-source workflows before retiring
    implementation-coupled cases.
  - [ ] Remove replaced source/registration/helper sets together and retain
    justified invalid-IR/ownership/fault/budget/width tests.
  - [ ] Update the case-level disposition ledger with exact replacements and
    reasons for retained focused tests.
- [ ] Meet the success criteria.
  - [ ] Every removed test has an equivalent observable behavior check or a
    documented obsolete contract.
  - [ ] Failure injection and inaccessible ownership/budget invariants retain
    meaningful coverage.
- [ ] Run the necessary tests.
  - [ ] Run replacement workflows before deletion, then the fresh full combined
    gate after migration.
  - [ ] Verify no stale CMake registrations, orphan fixtures or skipped required
    cases remain.
  - [ ] Compare coverage obligations and public outcomes rather than treating
    test-count reduction as success.

## Conditional extensions

Steps 40–50 require the recorded scope disposition described above.

<a id="step-40"></a>

### 40. Decide and, if selected, implement dynamic module loading

Backlog: D01. Dependencies: [21](#step-21), [29](#step-29).

- [ ] Perform the implementation or scope actions.
  - [ ] Record a static-only, native dynamic-library or explicit code-upgrade
    scope with supported replacement/lookup rules.
  - [ ] If selected, define image/descriptor validation, initialization and
    transactional publication using the concurrent code server.
  - [ ] Implement lookup removal/replacement while active frames, resolved
    handles and closures retain old code pins; define final unload ownership.
  - [ ] For omission, document static-only behavior and capability diagnostics
    without marking dynamic loading implemented.
- [ ] Meet the success criteria.
  - [ ] A recorded scope decision exists; selected dynamic behavior has safe
    image/term/code lifetimes.
  - [ ] Failure cannot replace a working module with a partially initialized
    image or unload active code.
- [ ] Run the necessary tests.
  - [ ] For selected loading, test valid/incompatible images,
    duplicate/replacement registration, live old closures/calls and final
    release.
  - [ ] Inject initialization/link/publication failures and race lookup with
    replacement/shutdown.
  - [ ] For static-only disposition, verify explicit rejection and ordinary
    static execution; retain the omission in the feature matrix.

<a id="step-41"></a>

### 41. Decide and, if needed, implement atom collection

Backlog: D02. Dependencies: [21](#step-21), [26](#step-26).

- [ ] Perform the implementation or scope actions.
  - [ ] Measure atom growth in selected projects and record whether bounded
    permanent storage is sufficient.
  - [ ] If collection is selected, enumerate term/host/module/frame/in-flight
    roots and define reclamation without identity reuse or dangling spellings.
  - [ ] Implement synchronized indexing/reclamation and accounting with safe
    lookup/intern interactions.
  - [ ] Otherwise retain bounded permanent storage and its exhaustion behavior
    as the documented policy.
- [ ] Meet the success criteria.
  - [ ] The atom lifetime policy is explicit and every retained
    identity/spelling remains valid.
  - [ ] Selected collection reclaims only unreachable storage and preserves
    concurrent lookup correctness.
- [ ] Run the necessary tests.
  - [ ] Stress atom creation, retained literals/host handles/code bindings and
    process termination.
  - [ ] For collection, race interning/lookup/publication with reclamation and
    inject workspace/allocation faults.
  - [ ] For permanent storage, test capacity rejection, retry with existing
    atoms and predictable accounting.

<a id="step-42"></a>

### 42. Select and implement behavior-changing attributes and transforms

Backlog: D03. Dependencies: [17](#step-17), [29](#step-29).

- [ ] Perform the implementation or scope actions.
  - [ ] Decide compile-option, parse-transform, on-load and other attribute
    support separately from existing inert/type metadata.
  - [ ] For selected transforms, define explicit invocation, executable/tool
    trust boundary, input/output limits, provenance and failure handling.
  - [ ] For selected on-load behavior, define registration visibility, startup
    order, failure rollback and allowed runtime operations.
  - [ ] Implement selected paths and retain located capability diagnostics for
    omitted attributes.
- [ ] Meet the success criteria.
  - [ ] Every selected attribute has documented executable behavior; unsupported
    forms cannot silently change compilation.
  - [ ] Transform/on-load failure preserves target isolation and prevents
    partial module/executable publication.
- [ ] Run the necessary tests.
  - [ ] Run real transformed/on-load source projects with deterministic outputs
    and original source locations.
  - [ ] Test malformed transform output, failed/missing tools, recursive
    invocation, resource limits and on-load exceptions.
  - [ ] Verify option precedence and unchanged behavior for inert metadata and
    unselected attributes.

<a id="step-43"></a>

### 43. Decide public stage interchange for a concrete consumer

Backlog: D04. Dependencies: [29](#step-29).

- [ ] Perform the implementation or scope actions.
  - [ ] Identify an actual external consumer and required producer stages;
    otherwise record interchange as deferred.
  - [ ] If selected, specify versioned owned-data output, source identity,
    resource ceilings and compatibility rules.
  - [ ] Implement only required producers under compiler/src/stage_writers/ with
    deterministic fixtures and explicit invocation.
  - [ ] Keep diagnostic IR/AST inspection outside the stable format contract and
    retain the D05 reader reservation restriction.
- [ ] Meet the success criteria.
  - [ ] Any selected public format has a concrete consumer and reviewable
    version/resource contract.
  - [ ] A deferred decision introduces no accidental public ABI or unused
    producer implementation.
- [ ] Run the necessary tests.
  - [ ] For selected producers, verify deterministic output, Unicode/provenance,
    version evolution and writer failure cleanup.
  - [ ] Exercise existing source/inspection workflows to ensure explicit public
    output does not change their contracts.
  - [ ] For deferral, verify documentation and capability inventory continue to
    describe internal owned stage exchange.

<a id="step-44"></a>

### 44. Preserve intermediate-reader directory reservations

Backlog: D05. Dependencies: [1](#step-1).

- [ ] Perform the implementation or scope actions.
  - [ ] Retain only compiler/src/stage_readers/{preprocessed,abstract,ir}/ as future source
    locations in the file map.
  - [ ] Record that AGENTS.md limits this plan to directory reservations; do not
    design decoders, schemas, command paths or reader implementations here.
  - [ ] Carry the unmet saved-stage-input request as deferred, with D04 as a
    related future owner.
  - [ ] Require a future explicit scope change before replacing this reservation
    with an implementation plan.
- [ ] Meet the success criteria.
  - [ ] D05 remains visible in the backlog while the repository instruction
    limiting reader planning is respected.
  - [ ] No reader capability is represented as implemented or promised by this
    plan.
- [ ] Run the necessary tests.
  - [ ] Review the file map and capability documentation for consistent
    reserved-only wording.
  - [ ] Confirm normal source compilation and inspection still make no
    saved-stage-input compatibility claim.
  - [ ] When a future scope change occurs, define its tests in that new reader
    plan; this reservation requires no decoder tests.

<a id="step-45"></a>

### 45. Decide minimal C or foreign interoperability for an actual caller

Backlog: D06. Dependencies: [18](#step-18), [29](#step-29).

- [ ] Perform the implementation or scope actions.
  - [ ] Identify a concrete external caller and the minimum functions,
    ownership, threading and error requirements; otherwise retain internal C++23
    only.
  - [ ] If selected, define versioned ABI-safe value/handle types and explicit
    allocation/release ownership.
  - [ ] Implement narrow adapters under the existing proposed interoperability
    owners, keeping compiler internals and STL types private.
  - [ ] Document calling conventions, runtime compatibility and unsupported
    conversions per target.
- [ ] Meet the success criteria.
  - [ ] Any exposed foreign interface has an independent consumer and explicit
    lifetime/error contract.
  - [ ] Deferral preserves the existing internal API policy without reinstating
    speculative wrappers.
- [ ] Run the necessary tests.
  - [ ] For selected adapters, compile/link/run independent consumers on each
    claimed calling convention/width.
  - [ ] Test invalid handles, wrong versions, ownership transfer, callback
    failures and teardown.
  - [ ] Verify generated ABI and generic C++ calls remain behaviorally
    equivalent after adding an adapter.

<a id="step-46"></a>

### 46. Select project schema extensions, profiles and exclusions

Backlog: D07. Dependencies: [29](#step-29).

- [ ] Perform the implementation or scope actions.
  - [ ] Record demand and selected extensions; define schema-version migration
    and compatibility with existing version-1 manifests.
  - [ ] Implement selected defaults/inheritance/profiles with explicit
    precedence against target and CLI options.
  - [ ] Add selected source exclusions while preserving normalized discovery
    order, duplicate handling and root confinement.
  - [ ] Update starter manifests and diagnostics; leave unselected extensions
    explicitly deferred.
- [ ] Meet the success criteria.
  - [ ] Selected project configuration resolves deterministically and existing
    manifests retain their documented behavior.
  - [ ] Unknown/incompatible schema fields and conflicting options produce
    actionable located errors.
- [ ] Run the necessary tests.
  - [ ] Run old/new project manifests with defaults, target overrides, profiles
    and CLI precedence combinations.
  - [ ] Test exclusions, nested paths, duplicate sources, Unicode, missing
    inputs and conflicting declarations.
  - [ ] Compare selected project builds with equivalent positional invocations
    and failed-target publication behavior.

<a id="step-47"></a>

### 47. Add selected project dependencies and imports

Backlog: D07. Dependencies: [46](#step-46).

- [ ] Perform the implementation or scope actions.
  - [ ] Define dependency/import identity, visibility, option inheritance and
    runtime/link artifact requirements.
  - [ ] Implement deterministic dependency graph expansion and topological
    scheduling with located cycle/missing-target diagnostics.
  - [ ] Propagate exports/artifacts through explicit edges while isolating
    target sessions and failure state.
  - [ ] Preserve whole-request publication rules and record when independent
    targets may finish after another failure.
- [ ] Meet the success criteria.
  - [ ] Selected dependency graphs build in a deterministic valid order and
    expose only declared imports.
  - [ ] Cycles, ambiguous identity and failed prerequisites cannot produce a
    misleading successful dependent artifact.
- [ ] Run the necessary tests.
  - [ ] Build diamond, deep-chain, independent and multi-root graphs through the
    public project CLI.
  - [ ] Test cycles, missing imports, duplicate module identity and conflicting
    target options.
  - [ ] Verify rebuild/retry after a failed prerequisite and compare final
    executables with equivalent flattened source builds.

<a id="step-48"></a>

### 48. Add reproducible package fetching when selected

Backlog: D07. Dependencies: [47](#step-47).

- [ ] Perform the implementation or scope actions.
  - [ ] Define package source schemes, immutable identities/checksums, lock
    data, cache ownership and offline behavior.
  - [ ] Implement bounded download/extraction with verified content and safe
    destination paths before making sources visible.
  - [ ] Integrate resolved packages into explicit project dependency graphs and
    preserve license/provenance metadata.
  - [ ] Define retry, corrupt-cache recovery and publication behavior for
    unavailable or changed remote content.
- [ ] Meet the success criteria.
  - [ ] Locked builds use the same verified source content and can use a valid
    offline cache.
  - [ ] Failed or unsafe packages cannot overwrite unrelated workspace files or
    publish partial dependency state.
- [ ] Run the necessary tests.
  - [ ] Use controlled local package fixtures for checksum mismatch, truncation,
    path traversal, duplicate entries and extraction limits.
  - [ ] Run cold/warm/offline builds and verify deterministic sources/artifacts
    and dependency diagnostics.
  - [ ] Test interrupted fetch, corrupt cache, lock mismatch and successful
    retry without relying on public network availability.

<a id="step-49"></a>

### 49. Add correct incremental caching and watch mode when selected

Backlog: D07. Dependencies: [46](#step-46).

- [ ] Perform the implementation or scope actions.
  - [ ] Define cache keys covering source/includes, macros/features,
    target/options, dependency identities, compiler/runtime ABI and toolchain
    versions.
  - [ ] Implement validated artifact reuse with transactional cache publication
    and bounded ownership/eviction.
  - [ ] Implement watch invalidation/debouncing and cancellation without
    publishing obsolete target results.
  - [ ] Document cache inspection/reset and failure recovery through explicit
    project CLI options.
- [ ] Meet the success criteria.
  - [ ] A warm or watched build produces the same behavior as a clean build for
    every tracked change.
  - [ ] Corrupt/stale entries and cancelled builds cannot replace valid outputs
    with obsolete or partial artifacts.
- [ ] Run the necessary tests.
  - [ ] Compare clean/warm outputs while changing includes, options, target
    width, ABI and generated inputs; include dependency changes when step 47 is
    selected.
  - [ ] Test rapid create/rename/delete events, cancelled compilation,
    clock-independent invalidation and cache corruption.
  - [ ] Run repeated failure/recovery cycles and verify diagnostics/order and
    successful artifact replacement.

<a id="step-50"></a>

### 50. Add deterministic parallel project execution when selected

Backlog: D07. Dependencies: [46](#step-46).

- [ ] Perform the implementation or scope actions.
  - [ ] Define per-target session isolation, dependency readiness, concurrency
    limits and shared cache/publication synchronization.
  - [ ] Implement bounded parallel target execution while keeping compiler state
    and diagnostics owned by their sessions.
  - [ ] Order aggregated diagnostics deterministically and propagate
    cancellation/failure through dependency edges.
  - [ ] Preserve safe artifact naming, collision detection and cache correctness
    under concurrent completion.
- [ ] Meet the success criteria.
  - [ ] Parallel and serial builds produce equivalent artifacts/results and
    stable diagnostics.
  - [ ] No shared-session races, partial dependent publication or abandoned work
    survive failure/cancellation.
- [ ] Run the necessary tests.
  - [ ] Compare serial/parallel independent targets across concurrency limits;
    add dependency graphs when step 47 is selected.
  - [ ] Inject target/link/cache failures and output collisions while other
    targets are finishing.
  - [ ] Run available race diagnostics and repeated parallel/cancellation
    stress; add cache/watch races when step 49 is selected.

## Final closure

<a id="step-51"></a>

### 51. Publish the final implementation and validation boundary

Backlog: F01, F02, F03, F04, F05, F06, F07, F08, F09, F10, F11, F12, F13, F14,
F16, F17, F18, F19, F20, F21, F22, F23, F24, F25, F26, F27, F28, F29, F30, F31,
F32, V01, V02, V03, V04. Dependencies: steps 1–39 and every selected optional
implementation from steps 40–50.

- [ ] Perform the implementation or scope actions.
  - [ ] Reconcile every remaining backlog checkbox with concrete
    implementation/evidence, or an explicit still-open owner; record each D-item
    as selected, deferred or deliberately omitted.
  - [ ] Run the full project/executable, corpus, stress/fault and available
    native/sanitizer matrix on the final revision.
  - [ ] Update contracts, examples, capability tables, archive,
    architecture/file maps and memory; retain all historical reference and
    validation records.
  - [ ] Publish reproducible commands, tool/source versions, per-platform
    outcomes and the actual compatibility boundary.
- [ ] Meet the success criteria.
  - [ ] All mandatory implementation steps and required validation cells are
    complete before declaring this plan complete.
  - [ ] Missing runners, selected unfinished optional work or unresolved
    compatibility failures remain visible open work; optional deferrals are
    never called implemented.
- [ ] Run the necessary tests.
  - [ ] Freshly configure/build compiler and runtime with testing, run complete
    CTest and unchanged Lizard/clang-tidy, and check formatting.
  - [ ] Run both drivers and all policies for representative projects; include
    ABI rejection, failed publication, recovery, small heaps and concurrent
    teardown.
  - [ ] Verify every evidence/contract link, fixture provenance, regenerated
    oracle result and backlog-to-step disposition.

