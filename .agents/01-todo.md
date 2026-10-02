# Missing features and completion backlog

Created 2026-09-30 from [00-finished.md](00-finished.md#outstanding-work-to-finish).
The 46-step immediate-term compiler milestone is complete. This backlog breaks
remaining work into features for separately chosen detailed plans.

**Feature IDs are references, not priority or implementation order.** Choose the
order of detailed planning yourself; resolve the noted dependencies in each plan.
All checkboxes describe future work. Creating this backlog does not start coding.

Extend the existing parser, type analysis, LLVM pipeline, runtime ownership and
registration services. Parsed syntax, API sketches and reporting placeholders do
not establish executable support. Explanations below describe the missing portion.

## Shared completion checklist

Apply these steps to each selected implementation feature, alongside its own list.

- [ ] Define supported behavior, exclusions, dependencies and observable failures.
- [ ] Resolve representation, ownership and ABI changes before enabling affected code.
- [ ] Add real-source CLI/runtime workflows and OTP comparisons where appropriate;
  retain justified invariant and injected-failure tests.
- [ ] Replace capability/catalog placeholders only for implemented semantics and
  update contracts, examples, architecture and file ownership where affected.
- [ ] Format code, document function/field intent, and pass the fresh combined Debug
  build, CTest and `check-quality` gate before a clean commit; do not weaken checks.

At the start of OTP-dependent implementation, follow [otp-reference.md](../docs/otp-reference.md)
to refresh official `maint-29` and synchronize checkout, pin, corpus and grammar
records. This backlog preserves the existing evidence; it does not refresh that pin.

## Executables and runtime data

### F01 — Production executable startup and linking

Meaning: produce a runnable Erlang program with runtime startup, rather than
requiring the separately written C++ harness used today.

- [ ] Define entrypoint selection, arguments, exit status and runtime startup options.
- [ ] Generate startup/module registration, initial execution and orderly shutdown;
  connect cooperative process execution when F22 becomes available.
- [ ] Drive Clang/platform linking with one matching target runtime and safe CLI/
  project output publication; replace the supported executable-output placeholder.
- [ ] Run emitted executables and verify missing-runtime, ABI, startup and link failures.

### F02 — Roots, safepoints and generated-code ABI evolution

Patternmatch step 2 delivered revision-2 checked call failure propagation and owned
immediate error payloads; see [the contract](../docs/generated-call-failures.md).
Heap roots, relocation and suspension remain open below.

Meaning: keep live values visible and valid when allocation, collection, calls,
exceptions or suspension can change where values are stored.

- [ ] Define roots for host Terms, generated temporaries, continuations, mailbox
  candidates and messages in transit, including registration and lifetime rules.
- [ ] Choose safepoint/relocation contracts before admitting movable terms; assess
  LLVM facilities while retaining project ownership of collector policy.
- [ ] Version affected descriptors/call boundaries and implement compiler/runtime
  root handling with target-derived widths and contained C++ exceptions.
- [ ] Verify live-value preservation across calls/allocation, then GC and suspension
  as implemented; reject incompatible consumers.

### F03 — Process heaps and TermFactory construction

Meaning: allocate actual process-owned storage and construct non-immediate terms;
today's heap budgets and constructor failures are only service boundaries.

- [ ] Implement checked backing allocation, accounting, growth and resource limits.
- [ ] Implement validated construction/destruction for selected term layouts,
  respecting C++ resources that cannot be moved as raw bytes.
- [ ] Connect constructors and host Terms to F02 ownership/root rules; enable each
  representation incrementally with rollback, allocation-failure and teardown checks.

### F04 — Process garbage collection

Meaning: reclaim unreachable process data while retaining live terms and their
references to shared or runtime-owned resources. Depends on F02/F03.

- [ ] Select collector policy and tracing rules for every enabled term layout.
- [ ] Trace all roots, preserve shared resources and update references if objects move.
- [ ] Integrate collection triggers, allocation retry and resource-limit failures.
- [ ] Stress live graphs, host handles, continuations and mailbox roots as available;
  verify cleanup and explicit C++ resource destruction.

### F05 — Graph copying and process isolation

Meaning: copy compound values between isolated heaps; existing immediate copies
do not establish safe copying of owned graphs.

- [ ] Define traversal, preserved internal sharing and immutable resource retention
  for supported layouts, including cross-runtime atom/identity handling.
- [ ] Implement rooted, bounded copies with destination budgets and rollback;
  extend `Term::copy_to` and heap addition using F02/F03.
- [ ] Verify independent lifetimes and failure cleanup, and reuse this service for
  message delivery without copying runtime-local IDs blindly.

### F06 — Atom storage and atom expressions

Meaning: execute named values such as `ok` and `true` through stable runtime-owned
identities, rather than only recognizing a tagged word's shape.

- [x] Implement validated spelling lookup/interning, stable non-recycled IDs,
  limits and transactional creation in one table per runtime.
- [x] Initialize generated spelling/slot bindings before module publication and
  retain needed roots; never emit compiler-assigned atom IDs.
- [x] Implement atom/boolean constructors and source lowering; verify deduplication,
  capacity failures and runtime isolation. See [step-3 validation](../docs/patternmatch-step3-validation.md).
- [ ] Add synchronized concurrent access before workers; current calls require host serialization.

### F07 — Process, port and reference identities

Meaning: validate real owned identities and their lifetimes; structural tag
recognition alone does not prove that a referenced entity exists.

- [ ] Define uniqueness, ownership and stale-identity behavior for each kind;
  distinguish a port identity from implementation of external I/O services.
- [ ] Implement constructors/lookups connected to concrete lifecycle owners, then
  integrate equality, host Terms, copying and routing where supported.
- [ ] Verify forged, stale and foreign identities; leave absent owner services
  explicitly unsupported rather than accepting arbitrary tagged words.

### F08 — Lists, tuples, maps and strings

Meaning: construct and access ordinary compound Erlang data. Strings use list
semantics; parsed aggregate syntax currently does not execute.

- [ ] Finalize layouts and ownership with F02/F03, covering improper lists and map
  key identity as well as ordinary containers.
- [ ] Implement constructors, access and updates; lower source construction and
  connect host Terms, graph copying, GC and later pattern access.
- [ ] Compare nested/empty values, invalid access and shared-value behavior with OTP.

### F09 — Binaries and bitstrings

Meaning: execute packed byte/bit data with correct segment interpretation,
tail bits and shared immutable storage lifetime.

- [ ] Finalize small/large storage, valid tail-bit rules, limits and shared ownership
  from the existing design; implement checked construction and release.
- [ ] Lower segment construction and required conversions without host-endianness
  assumptions; add extraction/matching with F13.
- [ ] Integrate tracing/copying and verify partial bytes, segment errors, large
  storage and last-owner release through real workflows.

### F10 — Arbitrary integers and integer arithmetic

Meaning: preserve Erlang integer results outside the small-integer range instead
of silently using machine overflow or wrapping.

- [ ] Implement owned bignum representation and literal construction using runtime
  arithmetic support, not compiler-side LLVM APInt at runtime.
- [ ] Implement arithmetic/bitwise operations, checked small-integer fast paths,
  promotion/demotion, division/shift behavior and Erlang failure outcomes.
- [ ] Lower operations with safe runtime fallbacks; compare large/negative/boundary
  inputs at O0/O2 and both widths, including resource and allocation failures.

### F11 — Floating-point values and arithmetic

Meaning: execute floats, conversions and mixed numeric operations with the chosen
Erlang behavior rather than merely preserving parsed float literals.

- [ ] Define representation, construction and the supported operation/conversion set.
- [ ] Implement runtime support and lowering with explicit errors and no LLVM
  assumptions that change numeric behavior; integrate tracing/copying and F12.
- [ ] Compare boundary, rounding, mixed integer/float and error cases with OTP.

## Executable language semantics

### F12 — Equality, comparisons and term ordering

Meaning: share correct exact/numeric equality and ordering across expressions,
map keys, patterns and guards as new representations become executable.

- [ ] Specify comparisons for each enabled representation, mixed numbers and nested
  terms; distinguish exact identity from numeric equality.
- [ ] Implement runtime comparison and lower operators with safe fast paths;
  reuse the appropriate rules in maps, patterns and guards.
- [ ] Verify cross-type, nested, numeric-boundary and equal-looking-but-distinct values.

### F13 — Pattern matching and bindings

Steps 4–6 provide scoped bindings, bounded normalization and executable immediate head matching, including sibling key/size legality. Later representations and dispatch remain pending.
See [step-5 validation](../docs/patternmatch-step5-validation.md).

Meaning: destructure values and bind/check variables beyond the distinct variable
or wildcard parameters currently accepted.

- [ ] Define scopes, repeated-variable equality, wildcards and mismatch outcomes
  for supported pattern forms.
- [ ] Implement checked matching/access for available representations and lower it
  while preserving existing bindings and rooted values on every path.
- [ ] Reuse matching in clauses and receive; verify nested patterns, binding
  visibility and mismatches through real source.

### F14 — Guards

Delivered steps 7–8: legal call resolution, immediate predicates/comparisons/queries, comma/semicolon guards and strict/lazy boolean control flow with semantic/infrastructure failure separation and structured badarg payloads; see docs/guard-control-flow.md. Ordered dispatch, other guard contexts and later representations remain open.

Meaning: decide whether clauses apply using restricted guard expressions and
their special failure rules.

- [ ] Validate allowed operations and grouping independently of parser acceptance.
- [ ] Implement guard BIFs/checks and short-circuit/failure control flow; require
  dominating representation proofs rather than trusting type annotations.
- [ ] Compare alternatives, runtime guard failures and invalid guard operations with OTP.

### F15 — Multiple function clauses

Meaning: select the first matching function clause instead of requiring exactly
one clause. Depends on matching and guards as supported by F13/F14.

- [x] Analyze clause-local bindings and ordered alternatives; merge inference conservatively.
- [x] Lower clause dispatch and the correct no-clause-match outcome.
- [x] Execute overlapping, fallback and failing clauses through local and remote calls.

Delivered for the admitted immediate domain by [patternmatch step 9](../docs/patternmatch-step9-validation.md). Later representation owners extend the shared matcher; recursion remains F21.

### F16 — Expression sequences and control flow

Meaning: execute several expressions and source branching rather than one
literal, parameter reference or direct call per function body.

- [x] Immediate body sequences and matches, including chained RHS-first semantics, exact rebinding checks and owned badmatch payloads: [patternmatch step 10](../docs/patternmatch-step10-validation.md). Other control contexts remain open.
- [ ] Inventory parsed constructs and select slices: sequences, matches, blocks,
  case/if, boolean control flow, then any chosen maybe/comprehension forms.
- [ ] Define scope, evaluation order and failure per slice; implement semantic
  analysis, inference joins and LLVM control flow using F13/F14/F20 as needed.
- [ ] Compare branch selection, visible bindings, nested evaluation and failures;
  keep unselected constructs rejected until separately completed.

### F17 — Record expansion and execution

Meaning: turn preserved record declarations/operations into their executable
data representation and field behavior.

- [ ] Resolve declarations, fields, defaults and errors after preprocessing.
- [ ] Implement construction, access, update and matching using the tuple
  representation, with correct evaluation order and failure semantics.
- [ ] Integrate binding/type analysis and verify included declarations, defaults,
  invalid fields and wrong-shaped values.

### F18 — Closures and function values

Meaning: create callable values that retain captured variables and code lifetime,
including the selected anonymous and named function forms.

- [ ] Define representation, arity, capture and module/code ownership contracts.
- [ ] Implement rooted capture environments with tracing/copying and retained code
  handles; lower construction/invocation, coordinating recursive forms with F21.
- [ ] Verify captures after creator return/GC, wrong arity, copied closures and
  retained module lifetime.

### F19 — Dynamic calls

Meaning: select functions at runtime through function values or module/function
names instead of resolving every call within the compilation batch.

- [ ] Define supported call forms, arity/argument checks and missing-function outcomes.
- [ ] Implement generic lookup/invocation with module pins and F06/F18 as needed;
  lower dynamic operands in source order with roots and error propagation.
- [ ] Verify successful calls, bad targets, missing exports and arity failures.

### F20 — Erlang exceptions

Patternmatch step 2 delivered checked nested-call transport for class error,
function_clause/badmatch reasons and immediate payloads, with clean retry and
infrastructure failures kept separate. Source raising and handlers remain open.

Meaning: implement error/exit/throw and source catch/try behavior without native
C++ exceptions escaping generated entry boundaries.

- [ ] Define exception classes, reasons, stack information and uncaught process outcomes.
- [ ] Select propagation/cleanup with F02 and continuations; implement raise,
  catch/try/after lowering and connect runtime operation failures.
- [ ] Verify nested handling, cleanup and failures across module/builtin calls,
  extending coverage to suspension as process execution becomes available.

### F21 — Recursion and proper tail calls

Meaning: allow recursive functions and long-running tail-recursive loops without
unbounded native stack growth.

- [ ] Extend batch call resolution and bounded inference to recursive components.
- [ ] Choose frame/tail-call handling compatible with roots, exceptions and process
  suspension; compare explicit continuations with LLVM coroutine mechanisms.
- [ ] Lower local/remote/mutual recursion and verify deep tail and non-tail cases,
  resource behavior and future reduction accounting.

## Processes and runtime services

### F22 — Cooperative process execution

Meaning: run generated code as resumable isolated processes; existing contexts
and scheduler records currently provide lifecycle bookkeeping only.

- [ ] Define entry/return/yield/exit protocol and rooted continuation ownership
  with F02/F21, including bounded work between scheduling opportunities.
- [ ] Implement create/start/resume/finish and generated-call integration; connect
  exceptions, identities and cleanup to the runtime execution service.
- [ ] Choose the supported spawn/exit/link/monitor slice before exposing its BIFs;
  verify isolation, yielding, resumption, failure and teardown.

### F23 — Scheduler workers and wakeups

Meaning: actually service multiple processes through workers and queues.
Scheduling policies in design sketches still need review.

- [ ] Define queue ownership, worker admission, reduction grants and scheduling policy.
- [ ] Implement run queues, workers, waiting/suspension and wakeups using F22;
  synchronize service access and shutdown.
- [ ] Service signals for waiting/suspended processes without clearing explicit
  suspension; verify fairness, lost-wakeup races and concurrent teardown.

### F24 — Signals and message sending

Meaning: transfer isolated values through ordered signals, including self-send,
instead of returning the current unavailable-send failure.

- [ ] Define admission, recipient validation, payload ownership and per-sender order
  across signal kinds using F07 and the process lifecycle contract.
- [ ] Enqueue every message into the signal inbox; use bounded owner-side handling
  and F05 copying before mailbox insertion, with rooted messages in transit.
- [ ] Lower send and distinguish acceptance from handling; verify order, heap
  isolation, recipient/copy failures and delivery to waiting/suspended processes.

### F25 — Selective receive and timeouts

Meaning: find a matching message, retain unmatched messages and wait or time out
without losing arrivals. Depends on F13–F15 and F22–F24 as applicable.

- [ ] Implement rooted receive cursors/candidates and ordered pattern/guard selection.
- [ ] Remove only the matched message; retain unmatched messages and cursor state.
- [ ] Implement tail/arrival handshake, suspension/resumption and timeouts; compare
  repeated scans, unmatched queues and arrival-versus-timeout races with OTP.

### F26 — Production builtin functions

Meaning: implement actual Erlang builtin behavior behind the existing generic
registry rather than only registered/unknown/unavailable dispatch boundaries.

- [ ] Inventory needed builtin families and select deliverable sets with explicit
  dependencies on terms, arithmetic, processes, messaging or modules.
- [ ] Implement argument validation, values and Erlang failures through the generic
  bridge; register implementations and enable source/guard use only where legal.
- [ ] Compare valid/invalid inputs and error classes with OTP, retaining unavailable
  reporting for signatures not yet implemented.

### F27 — Typed/native callables and conversions

Meaning: make selected typed C++ calls safe while preserving generic Term calls;
current typed templates are unverified proposals.

- [ ] Select concrete use cases and types; define conversion failures and lifetimes.
- [ ] Implement checked conversions/wrappers, retained module handles and explicit
  generic fallback without a speculative conversion registry.
- [ ] Keep STL values/C++ exceptions outside generated ABI boundaries; verify wrong
  types, expired lifetimes, callback failures and generic equivalence.

### F28 — Concurrent code-server access

Meaning: safely publish/resolve modules while workers run pinned calls; current
operations require host serialization.

- [ ] Define synchronization and publication visibility for frozen registries,
  descriptors, atom bindings and code ownership.
- [ ] Implement transactional concurrent publication/lookup with pins lasting
  through invocation and callable destruction; integrate worker/service shutdown.
- [ ] Stress duplicate registration, lookups, teardown and retained handles;
  dynamic unloading remains the separate D01 scope choice.

## Optimization and developer tooling

### F29 — Useful source-driven specialization

Meaning: extend existing bounded variants to remove real checks from newly
implemented source operations; today's subset has no removable checks.

- [ ] Identify operations/proven call profiles that yield a measurable benefit.
- [ ] Extend safe guards and lowering for implemented representations, preserving
  generic ABI fallback, count/work/growth limits and rollback; never trust specs alone.
- [ ] Compare real-source behavior, code size and compile cost with specialization
  on/off at O0/O2; use descriptive timings rather than noisy performance test gates.

### F30 — Debug information

Meaning: relate native instructions/frames to Erlang source and inspectable values.

- [ ] Define initial debugger support and source provenance across macros/includes.
- [ ] Emit LLVM debug metadata through object/link stages and describe term values
  and optimized/suspended-frame limitations.
- [ ] Verify source breakpoints, stack locations and supported value inspection on
  selected hosts before expanding the native matrix.

### F31 — Profiling

Meaning: explain generated-function and runtime/process costs; existing test
timing records are not a user-facing profiling facility.

- [ ] Select useful measurements, sampling/counter strategy and tool/output integration.
- [ ] Attribute costs with bounded overhead and deliberate enablement that preserves
  ordinary diagnostics and artifact behavior.
- [ ] Verify attribution and disabled mode; document overhead and interpretation limits.

### F32 — Link-time optimization

Meaning: optimize across module boundaries during native linking, beyond existing
per-module LLVM pipelines. Integrates with F01.

- [ ] Select supported LTO modes/toolchain combinations and compatibility rules.
- [ ] Wire bitcode/link inputs and options while preserving descriptors, startup,
  exports, runtime dependencies and ownership across optimization.
- [ ] Compare behavior/failures with ordinary linking and record size/build cost,
  supported targets and reproducible commands.

## Validation obligations

These close evidence gaps rather than add source-language features. Retain all
historical host/toolchain results with their original scope and revisions.

### V01 — Native platform matrix

Meaning: prove execution on required hosts; foreign object inspection is not a run.

- [ ] Prepare matching toolchains for Linux x86/x64/ARM/AArch64, macOS Apple Silicon
  and Windows x86; retain current Windows x64 coverage.
- [ ] Run fresh builds, quality and native runtime/generated-program workflows at
  O0/O2, including word widths, calling convention, failures and lifecycle.
- [ ] Fix platform issues and publish exact versions/pass/fail/skip counts, clearly
  retaining unavailable hosts as pending.

### V02 — Compiler/frontend sanitizers

Meaning: close memory/lifetime and undefined-behavior coverage gaps beyond the
existing runtime-only ASan results.

- [ ] Select compatible SDK/sanitizer configurations resolving recorded allocator
  and annotation ABI conflicts without suppressing checks.
- [ ] Run supported full compiler/frontend ASan, UBSan and LeakSanitizer workflows
  with ownership, AST lifetimes, stress and injected failures; fix findings.
- [ ] Record instrumentation scope explicitly, including SDK/generated-code limits,
  and separate unavailable configurations from passing runs.

### V03 — Broader OTP compatibility evidence

Meaning: broaden focused differential checks and upstream suite evidence using a
matching reference without confusing syntax acceptance with executable compatibility.

- [ ] Refresh/synchronize official `maint-29` using the reference procedure and build
  matching OTP, preserving historical revisions and results.
- [ ] Select/run applicable upstream Common Test suites with required hooks;
  distinguish upstream-only tests from ErlangAoT differential comparisons.
- [ ] Expand source comparisons for enabled features and publish exclusions,
  upstream failures, commands and the exact compatibility scope established.

### V04 — Remaining test migration

Meaning: replace implementation-coupled success tests only after equivalent public
behavior is covered, retaining cases public interfaces cannot reproduce.

- [ ] Audit the [case-level ledger](../docs/test-migration.md) against current CLI,
  native runtime and generated-program workflows; add missing equivalent coverage.
- [ ] Retire covered adapters/registrations/sources/helpers together; retain justified
  invalid-IR/handle, injected-failure, ownership and cross-width invariants.
- [ ] Run affected/full gates and update dispositions with evidence rather than
  treating test-count reduction as a completion goal.

## Optional or explicitly deferred scope

Choose scope before creating implementation plans for these items. They are missing
capabilities, not requirements for the already completed immediate-term milestone.

### D01 — Dynamic modules and code upgrades

Meaning: change executable modules at runtime. The project allows this to be
omitted or provided through separate native dynamic libraries.

- [ ] Choose static-only, native dynamic loading, or a defined upgrade model.
- [ ] If selected, specify images/descriptors, initialization, replacement and lookup
  removal while active calls/closures retain code pins; implement with F28.
- [ ] Verify incompatible images, active lifetimes, rollback and shutdown, or document
  deliberate omission and the static-only failure boundary.

### D02 — Atom collection

Meaning: reclaim unused atom storage; this is currently a reserved boundary without
an implemented table owner or selected collection policy.

- [ ] After F06, decide whether bounded permanent storage suffices or collection is needed.
- [ ] If selected, define roots across terms, hosts, code and in-flight work while
  preserving stable, non-recycled identities.
- [ ] Implement reclamation/index handling and verify retained bindings, concurrency
  and failure without dangling identities.

### D03 — Behavior-changing attributes and transforms

Meaning: support selected compilation/execution attributes beyond the inert
metadata and type declarations already accepted.

- [ ] Decide support separately for compile options, parse transforms, on-load
  behavior and other rejected constructs; explicit exclusions are valid outcomes.
- [ ] Define execution/tooling boundaries and failures for selected items; do not
  silently execute arbitrary transforms during parsing.
- [ ] Implement chosen semantics and real workflow coverage; keep other attributes rejected.

### D04 — Public stage interchange

Meaning: provide stable external intermediate-data formats beyond internal owned
objects and diagnostic inspection output.

- [ ] Identify a concrete consumer and select stages, versioning and resource limits.
- [ ] Specify validation/format and implement producers with compatibility fixtures.
- [ ] Add consumers only when needed, coordinating D05; do not declare inspection
  dumps a stable input ABI by accident.

### D05 — Intermediate-stage readers

Meaning: start compilation from saved preprocessed, abstract or IR data; today
only directory locations are reserved.

- [ ] Select needed stages and their input contracts, using D04 if public formats are needed.
- [ ] Implement bounded decoding/validation into owned internal data and an explicit
  invocation path; do not prebuild unused readers.
- [ ] Verify malformed/version-mismatched input and equivalent source-versus-stage outputs.

### D06 — C/FFI interoperability

Meaning: expose selected services outside internal C++23 interfaces; the earlier
C wrapper was intentionally removed and needs a concrete use case to return.

- [ ] Identify the external caller and minimum API, ownership and error contract.
- [ ] Design versioned ABI-safe types and implement only needed adapters.
- [ ] Test independent consumers and calling conventions while keeping compiler internals private.

### D07 — Project schema and build-workflow extensions

Meaning: extend version-1 manifests beyond explicit independent targets. Each
selected capability below can receive its own detailed plan.

- [ ] Select actual demand and define schema evolution/backward compatibility.
- [ ] Define defaults/inheritance and profiles with explicit option precedence.
- [ ] Define dependencies/imports with graph ordering and cycle diagnostics.
- [ ] Add source exclusions while preserving discovery/order guarantees.
- [ ] Add package fetching with reproducible identities and failure behavior.
- [ ] Add watch/cache with correct invalidation and safe artifact reuse/publication.
- [ ] Add parallel execution with session isolation and deterministic diagnostics.
- [ ] Implement and verify only selected extensions through real project CLI workflows.

## Existing references and future plans

- [Completed-work archive](00-finished.md), [architecture](arch.md), [file map](files.md),
  [compilation contract](../docs/compile.md), [validation](../docs/compile-validation.md).
- [Term design](../runtime/design/terms.md), [process design](../runtime/design/processes.md),
  [atom design](../runtime/design/atom_storage.md), [code-server design](../runtime/design/code_server.md).
- [Runtime lifecycle](../docs/runtime-lifecycle.md), [memory](../docs/runtime-memory.md),
  [services](../docs/runtime-services.md), [feature reporting](../docs/features.md).

Link each future detailed plan beside its stable feature ID. Check steps only when
supported by implementation/validation evidence. Record optional omissions as
omitted, rather than checking them off as implemented.
