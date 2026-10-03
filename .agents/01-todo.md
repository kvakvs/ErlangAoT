# Missing features and completion backlog

Created 2026-09-30 from
[00-finished.md](00-finished.md#outstanding-work-to-finish). The 46-step
immediate-term compiler milestone is complete. This backlog breaks remaining
work into stable feature owners. [Plan 11](11-plan.md) expands their remaining
work into small ordered steps with success criteria and tests.

**Feature IDs are references, not priority or implementation order.** Choose the
order of detailed planning yourself; resolve the noted dependencies in each
plan. Checked items identify delivered scoped slices; unchecked items remain
future work. Creating this backlog does not start coding.

Extend the existing parser, type analysis, LLVM pipeline, runtime ownership and
registration services. Parsed syntax, API sketches and reporting placeholders do
not establish executable support. Explanations below describe the missing
portion.

## Shared completion checklist

Apply these steps to each selected implementation feature, alongside its own
list.

- [ ] Define supported behavior, exclusions, dependencies and observable
  failures.
- [ ] Resolve representation, ownership and ABI changes before enabling affected
  code.
- [ ] Add real-source CLI/runtime workflows and OTP comparisons where
  appropriate; retain justified invariant and injected-failure tests.
- [ ] Replace capability/catalog placeholders only for implemented semantics and
  update contracts, examples, architecture and file ownership where affected.
- [ ] Format code, document function/field intent, and pass the fresh combined
  Debug build, CTest and `check-quality` gate before a clean commit; do not
  weaken checks.

At the start of OTP-dependent implementation, follow
[otp-reference.md](../docs/otp-reference.md) to refresh official `maint-29` and
synchronize checkout, pin, corpus and grammar records. This backlog preserves
the existing evidence; it does not refresh that pin.

## Executables and runtime data

### F01 — Production executable startup and linking

Expanded plan: [3](11-plan.md#step-3), [4](11-plan.md#step-4),
[5](11-plan.md#step-5), [6](11-plan.md#step-6), [7](11-plan.md#step-7),
[8](11-plan.md#step-8), [43](11-plan.md#step-43), [58](11-plan.md#step-58).

Meaning: produce a runnable Erlang program with runtime startup, rather than
requiring the separately written C++ harness used today.

- [ ] Define entrypoint selection, arguments, exit status and runtime startup
  options.
- [ ] Generate startup/module registration, initial execution and orderly
  shutdown; connect cooperative process execution when F22 becomes available.
- [ ] Drive Clang/platform linking with one matching target runtime and safe
  CLI/ project output publication; replace the supported executable-output
  placeholder.
- [ ] Run emitted executables and verify missing-runtime, ABI, startup and link
  failures.

### F02 — Roots, safepoints and generated-code ABI evolution

Expanded plan: [17](11-plan.md#step-17), [23](11-plan.md#step-23),
[26](11-plan.md#step-26), [51](11-plan.md#step-51).

Delivered stable-heap roots and owned results/errors for the admitted domain;
see [generated roots](../docs/generated-roots.md) and
[final validation](../docs/patternmatch-step20-validation.md). Relocation,
continuation/mailbox roots and suspension remain open.

- [x] Define and implement host/generated/registration roots and lifetime rules
  for admitted stable terms; preserve live values across calls/allocation and
  cleanup.
- [x] Version descriptors/call boundaries, retain target-derived widths, reject
  incompatible consumers and contain native exceptions.
- [ ] Define continuation, mailbox and transit roots with their concrete owners.
- [ ] Choose safepoint/relocation contracts and implement GC/suspension
  integration.

### F03 — Process heaps and TermFactory construction

Expanded plan: [23](11-plan.md#step-23), [25](11-plan.md#step-25),
[26](11-plan.md#step-26), [31](11-plan.md#step-31), [32](11-plan.md#step-32),
[33](11-plan.md#step-33), [42](11-plan.md#step-42).

Meaning: allocate process-owned storage and construct validated values. Stable
backing and the admitted scalar/container layouts are delivered; future layouts
and collector integration remain separate. See
[final validation](../docs/patternmatch-step20-validation.md).

- [x] Implement checked backing allocation, accounting, growth and resource
  limits.
- [x] Construct/destroy admitted integer/float/tuple/list/map/bitstring layouts,
  respecting explicit C++ resource ownership and transactional publication.
- [x] Connect host Terms and constructors to stable roots; verify rollback,
  allocation failures, retained values/errors and teardown.
- [ ] Extend validated construction/rooting for future identity/callable/native
  record layouts and integrate collection with F04.

### F04 — Process garbage collection

Expanded plan: [23](11-plan.md#step-23), [24](11-plan.md#step-24),
[25](11-plan.md#step-25), [26](11-plan.md#step-26), [27](11-plan.md#step-27),
[51](11-plan.md#step-51).

Meaning: reclaim unreachable process data while retaining live terms and their
references to shared or runtime-owned resources. Depends on F02/F03.

- [ ] Select collector policy and tracing rules for every enabled term layout.
- [ ] Trace all roots, preserve shared resources and update references if
  objects move.
- [ ] Integrate collection triggers, allocation retry and resource-limit
  failures.
- [ ] Stress live graphs, host handles, continuations and mailbox roots as
  available; verify cleanup and explicit C++ resource destruction.

### F05 — Graph copying and process isolation

Expanded plan: [28](11-plan.md#step-28), [45](11-plan.md#step-45).

Meaning: copy compound values between isolated heaps; existing immediate copies
do not establish safe copying of owned graphs.

- [ ] Define traversal, preserved internal sharing and immutable resource
  retention for supported layouts, including cross-runtime atom/identity
  handling.
- [ ] Implement rooted, bounded copies with destination budgets and rollback;
  extend `Term::copy_to` and heap addition using F02/F03.
- [ ] Verify independent lifetimes and failure cleanup, and reuse this service
  for message delivery without copying runtime-local IDs blindly.

### F06 — Atom storage and atom expressions

Expanded plan: [54](11-plan.md#step-54).

Meaning: execute named values such as `ok` and `true` through stable
runtime-owned identities, rather than only recognizing a tagged word's shape.

- [x] Implement validated spelling lookup/interning, stable non-recycled IDs,
  limits and transactional creation in one table per runtime.
- [x] Initialize generated spelling/slot bindings before module publication and
  retain needed roots; never emit compiler-assigned atom IDs.
- [x] Implement atom/boolean constructors and source lowering; verify
  deduplication, capacity failures and runtime isolation. See
  [step-3 validation](../docs/patternmatch-step3-validation.md).
- [ ] Add synchronized concurrent access before workers; current calls require
  host serialization.

### F07 — Process, port and reference identities

Expanded plan: [42](11-plan.md#step-42), [48](11-plan.md#step-48),
[53](11-plan.md#step-53).

Meaning: validate real owned identities and their lifetimes; structural tag
recognition alone does not prove that a referenced entity exists.

- [ ] Define uniqueness, ownership and stale-identity behavior for each kind;
  distinguish a port identity from implementation of external I/O services.
- [ ] Implement constructors/lookups connected to concrete lifecycle owners,
  then integrate equality, host Terms, copying and routing where supported.
- [ ] Verify forged, stale and foreign identities; leave absent owner services
  explicitly unsupported rather than accepting arbitrary tagged words.

### F08 — Lists, tuples, maps and strings

Expanded plan: [23](11-plan.md#step-23), [28](11-plan.md#step-28),
[39](11-plan.md#step-39).

Admitted construction, access, tuple/map updates and matching are delivered;
strings use proper lists. See [containers](../docs/container-matching.md),
[maps](../docs/map-matching.md) and
[final validation](../docs/patternmatch-step20-validation.md).

- [x] Define owned layouts, improper lists and exact map key identity.
- [x] Implement checked constructors/access/tuple-map updates and source
  lowering; reuse rooted host Terms, patterns and guard services.
- [x] Compare nested/empty values, invalid access, exact keys and retained
  values with OTP.
- [ ] Integrate graph copying and GC with F04/F05; additional list operations
  remain selected builtin-family work under F26.

### F09 — Binaries and bitstrings

Expanded plan: [23](11-plan.md#step-23), [28](11-plan.md#step-28).

Meaning: execute packed byte/bit data with correct segment interpretation, tail
bits and shared immutable storage lifetime.

Pattern/guard step 16 implements small/shared storage, checked numeric/UTF
construction and cursor extraction, retained tails, queries and comparisons. See
[the contract](../docs/bitstring-matching.md) and
[validation](../docs/patternmatch-step16-validation.md). Tracing, cross-process
copying and GC remain open with F04/F05; this does not close the whole F09
owner.

- [x] Finalize small/large storage, valid tail-bit rules, limits and shared
  ownership from the existing design; implement checked construction and
  release.
- [x] Lower segment construction and required conversions without
  host-endianness assumptions; add extraction/matching with F13.
- [ ] Integrate tracing/copying and verify partial bytes, segment errors, large
  storage and last-owner release through real workflows.

### F10 — Arbitrary integers and integer arithmetic

Expanded plan: [23](11-plan.md#step-23), [28](11-plan.md#step-28).

Delivered exact integers and checked arithmetic over the admitted domain; see
[integer contract](../docs/integer-matching.md) and
[final validation](../docs/patternmatch-step20-validation.md).

- [x] Implement owned bignums and literals using bounded runtime multiprecision.
- [x] Implement arithmetic/bitwise operations, small-integer fast paths,
  promotion/demotion, division/shift behavior and Erlang failures.
- [x] Lower with checked fallbacks; compare large/negative/boundary cases at
  O0/O2, allocation/resource failures and both-width IR/object layouts. Native
  32-bit is V01.
- [ ] Integrate graph copying and GC tracing with F04/F05.

### F11 — Floating-point values and arithmetic

Expanded plan: [23](11-plan.md#step-23), [28](11-plan.md#step-28),
[38](11-plan.md#step-38).

Delivered finite binary64 values, numeric operations/conversions and mixed
comparisons; see [float contract](../docs/float-matching.md) and
[final validation](../docs/patternmatch-step20-validation.md).

- [x] Define finite representation, construction and admitted
  operation/conversion set.
- [x] Implement checked runtime/lowering without unsafe LLVM numeric
  assumptions; share exact/mixed comparison rules with F12.
- [x] Compare boundaries, rounding, mixed integer/float inputs, signed zero and
  errors with OTP.
- [ ] Integrate tracing/copying with F04/F05; broader numeric additions need a
  selected scope.

## Executable language semantics

### F12 — Equality, comparisons and term ordering

Expanded plan: [31](11-plan.md#step-31), [32](11-plan.md#step-32),
[42](11-plan.md#step-42).

Delivered structural comparison for every admitted representation. See
[final validation](../docs/patternmatch-step20-validation.md); future identity,
callable/native-record representations still require their owners.

- [x] Specify exact/numeric equality and ordering, including mixed numbers and
  nested terms.
- [x] Implement checked runtime/lowering and reuse rules in maps, patterns and
  guards.
- [x] Verify cross-type, numeric boundaries and equal-looking-but-distinct
  keys/values.
- [ ] Extend comparison only as future representations become admitted.

### F13 — Pattern matching and bindings

Expanded plan: [9](11-plan.md#step-9), [13](11-plan.md#step-13),
[16](11-plan.md#step-16), [21](11-plan.md#step-21), [22](11-plan.md#step-22),
[46](11-plan.md#step-46).

Delivered function-head and body-match semantics for the admitted
scalar/container/ ordinary-record domain; see
[scoped matrix](../docs/patternmatch-matrix.md) and
[final validation](../docs/patternmatch-step20-validation.md).

- [x] Define scopes, aliases, exact repeated-variable equality, wildcards and
  mismatch outcomes, including map-key and binary-size binding rules.
- [x] Implement rooted checked matching/access with isolated candidate bindings
  and successful body publication over every admitted representation.
- [x] Reuse matching in ordered function clauses/body matches; verify nested
  patterns, visibility, failures and same-context retry through real source.
- [ ] Add case/if/maybe/comprehension, fun/catch and receive contexts with
  F16/F18/F20/F25; admit future representations through their owners.

### F14 — Guards

Expanded plan: [9](11-plan.md#step-9), [10](11-plan.md#step-10),
[52](11-plan.md#step-52).

Delivered the audited admitted-domain guard catalog and function-clause guard
control flow; see [guard services](../docs/guard-services.md) and
[final validation](../docs/patternmatch-step20-validation.md).

- [x] Validate operations/grouping, exact signatures, shadowing/imports and
  unreachable operands separately from availability and parsing.
- [x] Implement canonical-true, grouped/strict/lazy control flow and checked
  services; representation proofs dominate access, specs grant no authority.
- [x] Compare alternatives, reached semantic rejection, infrastructure failure
  propagation and invalid operations with OTP and native fault evidence.
- [ ] Enable self/0, node/0,1 and native is_record/1 only with F07/F17/F22/F26.
- [ ] Add guard contexts outside function clauses with F16/F18/F20/F25 and
  positive identity/function classifications with F07/F18.

### F15 — Multiple function clauses

Delivered ordered selection for the full admitted domain; see
[final validation](../docs/patternmatch-step20-validation.md). Recursion remains
F21.

- [x] Analyze clause-local bindings and ordered alternatives; join inference
  conservatively.
- [x] Lower checked dispatch and the correct function_clause outcome.
- [x] Execute overlapping, fallback and failing clauses through local/remote
  calls.

### F16 — Expression sequences and control flow

Expanded plan: [9](11-plan.md#step-9), [10](11-plan.md#step-10),
[16](11-plan.md#step-16), [21](11-plan.md#step-21), [22](11-plan.md#step-22).

Delivered body sequences/matches over every admitted representation, including
RHS-first chains, exact rebinding, owned badmatch and rooted construction; see
[final validation](../docs/patternmatch-step20-validation.md).

- [x] Implement sequences/matches and strict/lazy boolean expression evaluation,
  preserving order, bindings and failures through real source/native workflows.
- [ ] Select remaining blocks, case/if and maybe/comprehension slices.
- [ ] Define scope/evaluation/failure, inference joins and LLVM control flow
  using F13/F14/F20; compare branch selection, visibility and errors with OTP.

### F17 — Record expansion and execution

Expanded plan: [29](11-plan.md#step-29), [30](11-plan.md#step-30),
[31](11-plan.md#step-31).

Meaning: turn preserved record declarations/operations into their executable
representation and field behavior.

- [x] Resolve ordinary declarations, fields, defaults and errors after
  preprocessing.
- [x] Implement ordinary construction, access and matching through tuples,
  preserving evaluation order and failure semantics; integrate binding/type
  traversal. See [step 17](../docs/patternmatch-step17-validation.md).
- [ ] Implement record updates, record_info, and native/qualified/inferred
  records.
- [ ] Complete their additional declaration, binding/type and execution rules.

### F18 — Closures and function values

Expanded plan: [32](11-plan.md#step-32), [33](11-plan.md#step-33),
[34](11-plan.md#step-34).

Meaning: create callable values that retain captured variables and code
lifetime, including the selected anonymous and named function forms.

- [ ] Define representation, arity, capture and module/code ownership contracts.
- [ ] Implement rooted capture environments with tracing/copying and retained
  code handles; lower construction/invocation, coordinating recursive forms with
  F21.
- [ ] Verify captures after creator return/GC, wrong arity, copied closures and
  retained module lifetime.

### F19 — Dynamic calls

Expanded plan: [35](11-plan.md#step-35).

Meaning: select functions at runtime through function values or module/function
names instead of resolving every call within the compilation batch.

- [ ] Define supported call forms, arity/argument checks and missing-function
  outcomes.
- [ ] Implement generic lookup/invocation with module pins and F06/F18 as
  needed; lower dynamic operands in source order with roots and error
  propagation.
- [ ] Verify successful calls, bad targets, missing exports and arity failures.

### F20 — Erlang exceptions

Expanded plan: [11](11-plan.md#step-11), [12](11-plan.md#step-12),
[13](11-plan.md#step-13), [14](11-plan.md#step-14), [15](11-plan.md#step-15).

Patternmatch step 2 delivered checked nested-call transport for class error,
function_clause and admitted service reasons with owned scalar/container
payloads, clean retry and separate infrastructure failures. Source raising and
handlers remain open.

Meaning: implement error/exit/throw and source catch/try behavior without native
C++ exceptions escaping generated entry boundaries.

- [ ] Define exception classes, reasons, stack information and uncaught process
  outcomes.
- [ ] Select propagation/cleanup with F02 and continuations; implement raise,
  catch/try/after lowering and connect runtime operation failures.
- [ ] Verify nested handling, cleanup and failures across module/builtin calls,
  extending coverage to suspension as process execution becomes available.

### F21 — Recursion and proper tail calls

Expanded plan: [17](11-plan.md#step-17), [18](11-plan.md#step-18),
[19](11-plan.md#step-19), [20](11-plan.md#step-20), [34](11-plan.md#step-34).

Meaning: allow recursive functions and long-running tail-recursive loops without
unbounded native stack growth.

- [ ] Extend batch call resolution and bounded inference to recursive
  components.
- [ ] Choose frame/tail-call handling compatible with roots, exceptions and
  process suspension; compare explicit continuations with LLVM coroutine
  mechanisms.
- [ ] Lower local/remote/mutual recursion and verify deep tail and non-tail
  cases, resource behavior and future reduction accounting.

## Processes and runtime services

### F22 — Cooperative process execution

Expanded plan: [17](11-plan.md#step-17), [43](11-plan.md#step-43),
[44](11-plan.md#step-44), [48](11-plan.md#step-48), [49](11-plan.md#step-49).

Meaning: run generated code as resumable isolated processes; existing contexts
and scheduler records currently provide lifecycle bookkeeping only.

- [ ] Define entry/return/yield/exit protocol and rooted continuation ownership
  with F02/F21, including bounded work between scheduling opportunities.
- [ ] Implement create/start/resume/finish and generated-call integration;
  connect exceptions, identities and cleanup to the runtime execution service.
- [ ] Choose the supported spawn/exit/link/monitor slice before exposing its
  BIFs; verify isolation, yielding, resumption, failure and teardown.

### F23 — Scheduler workers and wakeups

Expanded plan: [56](11-plan.md#step-56), [57](11-plan.md#step-57).

Meaning: actually service multiple processes through workers and queues.
Scheduling policies in design sketches still need review.

- [ ] Define queue ownership, worker admission, reduction grants and scheduling
  policy.
- [ ] Implement run queues, workers, waiting/suspension and wakeups using F22;
  synchronize service access and shutdown.
- [ ] Service signals for waiting/suspended processes without clearing explicit
  suspension; verify fairness, lost-wakeup races and concurrent teardown.

### F24 — Signals and message sending

Expanded plan: [45](11-plan.md#step-45).

Meaning: transfer isolated values through ordered signals, including self-send,
instead of returning the current unavailable-send failure.

- [ ] Define admission, recipient validation, payload ownership and per-sender
  order across signal kinds using F07 and the process lifecycle contract.
- [ ] Enqueue every message into the signal inbox; use bounded owner-side
  handling and F05 copying before mailbox insertion, with rooted messages in
  transit.
- [ ] Lower send and distinguish acceptance from handling; verify order, heap
  isolation, recipient/copy failures and delivery to waiting/suspended
  processes.

### F25 — Selective receive and timeouts

Expanded plan: [46](11-plan.md#step-46), [47](11-plan.md#step-47),
[57](11-plan.md#step-57).

Meaning: find a matching message, retain unmatched messages and wait or time out
without losing arrivals. Depends on F13–F15 and F22–F24 as applicable.

- [ ] Implement rooted receive cursors/candidates and ordered pattern/guard
  selection.
- [ ] Remove only the matched message; retain unmatched messages and cursor
  state.
- [ ] Implement tail/arrival handshake, suspension/resumption and timeouts;
  compare repeated scans, unmatched queues and arrival-versus-timeout races with
  OTP.

### F26 — Production builtin functions

Expanded plan: [2](11-plan.md#step-2), [4](11-plan.md#step-4),
[36](11-plan.md#step-36), [37](11-plan.md#step-37), [38](11-plan.md#step-38),
[39](11-plan.md#step-39), [40](11-plan.md#step-40), [50](11-plan.md#step-50).

Meaning: implement actual builtin behavior behind the generic registry. The
compiler guard-service slice is delivered, while generic production registration
remains open.

- [x] Audit all 81 source catalog rows; implement 77 on admitted values with
  checked validation/results/failures, executable mappings and explicit gates
  for four owners. See [guard services](../docs/guard-services.md) and
  [final validation](../docs/patternmatch-step20-validation.md).
- [ ] Select additional builtin families and their concrete runtime
  dependencies.
- [ ] Implement/register production families through the generic bridge and
  enable source use only where legal; compiler-authorized services do not close
  this work.
- [ ] Compare generic bridge valid/invalid calls and errors with OTP while
  retaining unavailable diagnostics for missing services.

### F27 — Typed/native callables and conversions

Expanded plan: [41](11-plan.md#step-41).

Meaning: make selected typed C++ calls safe while preserving generic Term calls;
current typed templates are unverified proposals.

- [ ] Select concrete use cases and types; define conversion failures and
  lifetimes.
- [ ] Implement checked conversions/wrappers, retained module handles and
  explicit generic fallback without a speculative conversion registry.
- [ ] Keep STL values/C++ exceptions outside generated ABI boundaries; verify
  wrong types, expired lifetimes, callback failures and generic equivalence.

### F28 — Concurrent code-server access

Expanded plan: [55](11-plan.md#step-55).

Meaning: safely publish/resolve modules while workers run pinned calls; current
operations require host serialization.

- [ ] Define synchronization and publication visibility for frozen registries,
  descriptors, atom bindings and code ownership.
- [ ] Implement transactional concurrent publication/lookup with pins lasting
  through invocation and callable destruction; integrate worker/service
  shutdown.
- [ ] Stress duplicate registration, lookups, teardown and retained handles;
  dynamic unloading remains the separate D01 scope choice.

## Optimization and developer tooling

### F29 — Useful source-driven specialization

Expanded plan: [59](11-plan.md#step-59).

Meaning: extend existing bounded variants to remove real checks from newly
implemented source operations; today's subset has no removable checks.

- [ ] Identify operations/proven call profiles that yield a measurable benefit.
- [ ] Extend safe guards and lowering for implemented representations,
  preserving generic ABI fallback, count/work/growth limits and rollback; never
  trust specs alone.
- [ ] Compare real-source behavior, code size and compile cost with
  specialization on/off at O0/O2; use descriptive timings rather than noisy
  performance test gates.

### F30 — Debug information

Expanded plan: [60](11-plan.md#step-60).

Meaning: relate native instructions/frames to Erlang source and inspectable
values.

- [ ] Define initial debugger support and source provenance across
  macros/includes.
- [ ] Emit LLVM debug metadata through object/link stages and describe term
  values and optimized/suspended-frame limitations.
- [ ] Verify source breakpoints, stack locations and supported value inspection
  on selected hosts before expanding the native matrix.

### F31 — Profiling

Expanded plan: [61](11-plan.md#step-61).

Meaning: explain generated-function and runtime/process costs; existing test
timing records are not a user-facing profiling facility.

- [ ] Select useful measurements, sampling/counter strategy and tool/output
  integration.
- [ ] Attribute costs with bounded overhead and deliberate enablement that
  preserves ordinary diagnostics and artifact behavior.
- [ ] Verify attribution and disabled mode; document overhead and interpretation
  limits.

### F32 — Link-time optimization

Expanded plan: [62](11-plan.md#step-62).

Meaning: optimize across module boundaries during native linking, beyond
existing per-module LLVM pipelines. Integrates with F01.

- [ ] Select supported LTO modes/toolchain combinations and compatibility rules.
- [ ] Wire bitcode/link inputs and options while preserving descriptors,
  startup, exports, runtime dependencies and ownership across optimization.
- [ ] Compare behavior/failures with ordinary linking and record size/build
  cost, supported targets and reproducible commands.

## Validation obligations

These close evidence gaps rather than add source-language features. Retain all
historical host/toolchain results with their original scope and revisions.

### V01 — Native platform matrix

Expanded plan: [63](11-plan.md#step-63), [64](11-plan.md#step-64),
[65](11-plan.md#step-65), [66](11-plan.md#step-66).

Meaning: prove execution on required hosts; foreign object inspection is not a
run.

- [ ] Prepare matching toolchains for Linux x86/x64/ARM/AArch64, macOS Apple
  Silicon and Windows x86; retain current Windows x64 coverage.
- [ ] Run fresh builds, quality and native runtime/generated-program workflows
  at O0/O2, including word widths, calling convention, failures and lifecycle.
- [ ] Fix platform issues and publish exact versions/pass/fail/skip counts,
  clearly retaining unavailable hosts as pending.

### V02 — Compiler/frontend sanitizers

Expanded plan: [67](11-plan.md#step-67), [68](11-plan.md#step-68).

Meaning: close memory/lifetime and undefined-behavior coverage gaps beyond the
existing runtime-only ASan results.

- [ ] Select compatible SDK/sanitizer configurations resolving recorded
  allocator and annotation ABI conflicts without suppressing checks.
- [ ] Run supported full compiler/frontend ASan, UBSan and LeakSanitizer
  workflows with ownership, AST lifetimes, stress and injected failures; fix
  findings.
- [ ] Record instrumentation scope explicitly, including SDK/generated-code
  limits, and separate unavailable configurations from passing runs.

### V03 — Broader OTP compatibility evidence

Expanded plan: [1](11-plan.md#step-1), [2](11-plan.md#step-2),
[58](11-plan.md#step-58), [69](11-plan.md#step-69).

Meaning: broaden focused differential checks and upstream suite evidence using a
matching reference without confusing syntax acceptance with executable
compatibility.

- [ ] Refresh/synchronize official `maint-29` using the reference procedure and
  build matching OTP, preserving historical revisions and results.
- [ ] Select/run applicable upstream Common Test suites with required hooks;
  distinguish upstream-only tests from ErlangAoT differential comparisons.
- [ ] Expand source comparisons for enabled features and publish exclusions,
  upstream failures, commands and the exact compatibility scope established.

### V04 — Remaining test migration

Expanded plan: [8](11-plan.md#step-8), [70](11-plan.md#step-70).

Meaning: replace implementation-coupled success tests only after equivalent
public behavior is covered, retaining cases public interfaces cannot reproduce.

- [ ] Audit the [case-level ledger](../docs/test-migration.md) against current
  CLI, native runtime and generated-program workflows; add missing equivalent
  coverage.
- [ ] Retire covered adapters/registrations/sources/helpers together; retain
  justified invalid-IR/handle, injected-failure, ownership and cross-width
  invariants.
- [ ] Run affected/full gates and update dispositions with evidence rather than
  treating test-count reduction as a completion goal.

## Optional or explicitly deferred scope

Choose scope before creating implementation plans for these items. They are
missing capabilities, not requirements for the already completed immediate-term
milestone.

### D01 — Dynamic modules and code upgrades

Expanded plan: [71](11-plan.md#step-71).

Meaning: change executable modules at runtime. The project allows this to be
omitted or provided through separate native dynamic libraries.

- [ ] Choose static-only, native dynamic loading, or a defined upgrade model.
- [ ] If selected, specify images/descriptors, initialization, replacement and
  lookup removal while active calls/closures retain code pins; implement with
  F28.
- [ ] Verify incompatible images, active lifetimes, rollback and shutdown, or
  document deliberate omission and the static-only failure boundary.

### D02 — Atom collection

Expanded plan: [72](11-plan.md#step-72).

Meaning: reclaim unused atom storage; this is currently a reserved boundary
without an implemented table owner or selected collection policy.

- [ ] After F06, decide whether bounded permanent storage suffices or collection
  is needed.
- [ ] If selected, define roots across terms, hosts, code and in-flight work
  while preserving stable, non-recycled identities.
- [ ] Implement reclamation/index handling and verify retained bindings,
  concurrency and failure without dangling identities.

### D03 — Behavior-changing attributes and transforms

Expanded plan: [73](11-plan.md#step-73).

Meaning: support selected compilation/execution attributes beyond the inert
metadata and type declarations already accepted.

- [ ] Decide support separately for compile options, parse transforms, on-load
  behavior and other rejected constructs; explicit exclusions are valid
  outcomes.
- [ ] Define execution/tooling boundaries and failures for selected items; do
  not silently execute arbitrary transforms during parsing.
- [ ] Implement chosen semantics and real workflow coverage; keep other
  attributes rejected.

### D04 — Public stage interchange

Expanded plan: [74](11-plan.md#step-74).

Meaning: provide stable external intermediate-data formats beyond internal owned
objects and diagnostic inspection output.

- [ ] Identify a concrete consumer and select stages, versioning and resource
  limits.
- [ ] Specify validation/format and implement producers with compatibility
  fixtures.
- [ ] Add consumers only when needed, coordinating D05; do not declare
  inspection dumps a stable input ABI by accident.

### D05 — Intermediate-stage readers

Expanded plan: [75](11-plan.md#step-75).

Meaning: start compilation from saved preprocessed, abstract or IR data; today
only directory locations are reserved.

- [ ] Select needed stages and their input contracts, using D04 if public
  formats are needed.
- [ ] Implement bounded decoding/validation into owned internal data and an
  explicit invocation path; do not prebuild unused readers.
- [ ] Verify malformed/version-mismatched input and equivalent
  source-versus-stage outputs.

### D06 — C/FFI interoperability

Expanded plan: [76](11-plan.md#step-76).

Meaning: expose selected services outside internal C++23 interfaces; the earlier
C wrapper was intentionally removed and needs a concrete use case to return.

- [ ] Identify the external caller and minimum API, ownership and error
  contract.
- [ ] Design versioned ABI-safe types and implement only needed adapters.
- [ ] Test independent consumers and calling conventions while keeping compiler
  internals private.

### D07 — Project schema and build-workflow extensions

Expanded plan: [77](11-plan.md#step-77).

Meaning: extend version-1 manifests beyond explicit independent targets. Each
selected capability below can receive its own detailed plan.

- [ ] Select actual demand and define schema evolution/backward compatibility.
- [ ] Define defaults/inheritance and profiles with explicit option precedence.
- [ ] Define dependencies/imports with graph ordering and cycle diagnostics.
- [ ] Add source exclusions while preserving discovery/order guarantees.
- [ ] Add package fetching with reproducible identities and failure behavior.
- [ ] Add watch/cache with correct invalidation and safe artifact
  reuse/publication.
- [ ] Add parallel execution with session isolation and deterministic
  diagnostics.
- [ ] Implement and verify only selected extensions through real project CLI
  workflows.

## Existing references and future plans

- [Completed-work archive](00-finished.md), [architecture](arch.md),
  [file map](files.md), [compilation contract](../docs/compile.md),
  [validation](../docs/compile-validation.md).
- [Term design](../runtime/design/terms.md),
  [process design](../runtime/design/processes.md),
  [atom design](../runtime/design/atom_storage.md),
  [code-server design](../runtime/design/code_server.md).
- [Runtime lifecycle](../docs/runtime-lifecycle.md),
  [memory](../docs/runtime-memory.md), [services](../docs/runtime-services.md),
  [feature reporting](../docs/features.md).

Link each future detailed plan beside its stable feature ID. Check steps only
when supported by implementation/validation evidence. Record optional omissions
as omitted, rather than checking them off as implemented.
