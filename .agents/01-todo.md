# Missing features and completion backlog

Stable feature owners for the remaining work (created 2026-09-30 from
[outstanding work](00-finished.md#outstanding-work-to-finish), compacted
2026-10-04). [Plan 11](11-plan.md) orders it into small steps. IDs are
references, not priorities. Checked items are delivered scoped slices; parsed
syntax, API sketches and placeholders do not count as support.

## Shared completion checklist

For each implemented slice: define behavior, exclusions and failures; settle
representation, ownership and ABI first; test through real sources, CLI,
executables and OTP comparisons, keeping justified invariant and fault tests;
replace placeholders only for implemented semantics; update docs, `arch.md`,
`files.md`; format and pass the fresh combined Debug gate without weakened
checks. OTP-dependent work starts by refreshing `maint-29` per
[otp-reference.md](../docs/otp-reference.md).

## Executables and runtime data

### F01 — Production executable startup and linking

Plan: [3](11-plan.md#step-3)–[8](11-plan.md#step-8), [43](11-plan.md#step-43),
[58](11-plan.md#step-58). Runnable programs without a hand-written harness.

- [x] Entry selection, argv, exit status (`docs/executables.md`, steps 3, 3A).
- [x] Startup object, registration and ordered shutdown for one context (step 5).
- [x] Clang linking for positional and project builds with staged publication
  (steps 6, 6A, 7); missing-runtime/ABI/link failures tested.
- [x] Connect startup to cooperative process execution (F22, step 43).
- [x] Runtime options `--max-atoms` and `ERLANG_AOT_FLAGS` (step 27 follow-up).
- [ ] `--args-file` options file like `vm.args` (reserved; reports not
  implemented).

### F02 — Roots, safepoints and generated-code ABI evolution

Plan: [8E](11-plan.md#step-8e), [8F](11-plan.md#step-8f),
[17](11-plan.md#step-17), [23](11-plan.md#step-23), [26](11-plan.md#step-26),
[51](11-plan.md#step-51). Contract: [frames and transfers](../docs/abi.md#frames-and-transfers).

- [x] Host/generated/registration roots and owned results/errors; versioned
  descriptors, width checks, contained native exceptions.
- [x] ERTS host model: raw-word `Term`s, handoff and error-payload root words,
  explicit root span, segmented process root stack (8E, 8F).
- [x] One flat process stack of explicit frames replaces the segmented root
  stack; values live across calls spill to raw frame slots (step 19).
- [x] Continuation roots: frame term slots, live registers and the failure
  channel enumerated, collectable inside a declared `SafePoint` (step 23).
- [ ] Mailbox and transit roots with concrete owners.
- [x] Generated-code safepoints (function entry, comprehension loop heads) with
  term spills reloaded from term slots after collection (steps 24, 26).

### F03 — Process heaps and TermFactory construction

Plan: [8A](11-plan.md#step-8a)–[8I](11-plan.md#step-8i),
[23](11-plan.md#step-23), [26](11-plan.md#step-26), [31](11-plan.md#step-31)–[33](11-plan.md#step-33),
[42](11-plan.md#step-42). Contract: [runtime-heap.md](../docs/runtime-heap.md).

- [x] Checked allocation, accounting, budgets and transactional publication of
  admitted integer/float/tuple/list/map/bitstring layouts.
- [x] Header-parsed word layout, heap walker and `verify()`, admission by owned
  range and header (8B–8D).
- [x] One heap block per process plus heap fragments; word alignment only (8G).
- [x] Close the heap rework with re-measurement and docs (8I).
- [x] Layouts for identities, closures and native records (steps 31B, 32, 42).

### F04 — Process garbage collection

Plan: [8A](11-plan.md#step-8a), [8C](11-plan.md#step-8c),
[8H](11-plan.md#step-8h), [8I](11-plan.md#step-8i), [23](11-plan.md#step-23),
[24](11-plan.md#step-24), [26](11-plan.md#step-26), [27](11-plan.md#step-27),
[27A](11-plan.md#step-27a),
[51](11-plan.md#step-51).

- [x] Collector policy (full Cheney copy, ERTS sizing) and traced words for
  every admitted layout (8A, 8C).
- [x] Copying collector on explicit host request: rewrite roots, sweep off-heap
  list, merge fragments, grow/shrink along the ERTS sizes (8H).
- [x] Triggers from generated code: fragments, off-heap pressure; allocation
  stays a critical section, so no retry (24, 26).
- [x] Memory exhaustion as a defined failure: no default cap, host refusal is
  `out_of_memory`; opt-in per-process budgets fail after collection (27).
- [x] Optional runtime-wide memory limit (uncapped by default) and
  program-facing caps `--max-heap`, `--max-stack`, `--max-memory` (27A).
- [ ] `erlang:garbage_collect/0` with the builtins.
- [ ] Stress with continuations and mailbox roots as they arrive.
- [ ] Optional later: generational old heap with minor collections.

### F05 — Graph copying and process isolation

Plan: [8H](11-plan.md#step-8h), [28](11-plan.md#step-28),
[45](11-plan.md#step-45). Graphs copy between heaps of one runtime (28).

- [x] Sharing-preserving copy with destination budget and rollback;
  `Term::copy_to` and `ProcessHeap::add` (28).
- [ ] Cross-runtime atom/identity handling (cross-runtime copies are
  `wrong_owner`); reuse for message delivery (45).
- [x] Independent lifetimes and failure cleanup verified (28).

### F06 — Atom storage and atom expressions

Plan: [54](11-plan.md#step-54).

- [x] One bounded table per runtime, stable non-recycled IDs, generated
  spelling/slot bindings before publication, atom/boolean literals.
- [ ] Synchronized concurrent access before workers.

### F07 — Process, port and reference identities

Plan: [42](11-plan.md#step-42), [48](11-plan.md#step-48),
[53](11-plan.md#step-53).

- [x] Uniqueness, ownership and stale-identity rules for pids and references
  (step 42, [terms](../docs/terms.md#pids-and-references)).
- [ ] Ports separate from I/O services (step 53).
- [x] Owned constructors, equality, order, printing, host Terms, copying (step 42).
- [ ] Routing by pid (steps 45, 48).
- [x] Reject forged, stale and foreign identities (step 42).

### F08 — Lists, tuples, maps and strings

Plan: [23](11-plan.md#step-23), [27B](11-plan.md#step-27b), [27C](11-plan.md#step-27c),
[27D](11-plan.md#step-27d), [28](11-plan.md#step-28),
[39](11-plan.md#step-39). Contract: [terms](../docs/terms.md#tuples-lists-strings).

- [x] Layouts, improper lists, exact map keys; checked construction, access,
  updates and matching compared with OTP.
- [x] No list length cap and no comparison work cap (27B).
- [x] Tuple arity limit of OTP, 16,777,215 (27C).
- [x] No map size or key-work caps; O(n log n) map construction (27D).
- [x] GC/copying integration (F04/F05, 28).
- [x] `++`/`--`, `tuple_to_list`/`list_to_tuple`, `setelement`, `make_tuple`
  (step 37).
- [x] `lists` subset (step 39): `append`, `filter`, `foldl`, `foldr`,
  `keyfind`, `map`, `member`, `nth`, `reverse`, `seq`, `sort`; `maps`
  subset.

### F09 — Binaries and bitstrings

Plan: [8B](11-plan.md#step-8b), [23](11-plan.md#step-23),
[28](11-plan.md#step-28). Contract: [terms](../docs/terms.md#bitstrings).

- [x] Inline heap binaries up to 64 bytes and shared `refc_binary` buffers on a
  per-process off-heap list (8B); checked segments, cursors, tails, queries.
- [x] Collector sweep of dead off-heap cells (8H).
- [x] Cross-process copying shares the buffer; each process charges it once (28).

### F10 — Arbitrary integers and integer arithmetic

Plan: [23](11-plan.md#step-23), [27E](11-plan.md#step-27e), [28](11-plan.md#step-28).

- [x] Owned bignums, exact arithmetic/bitwise operations, fast paths,
  promotion/demotion, checked lowering compared with OTP.
- [x] ERTS size limit (4,194,240 bits on 64-bit) with `error:system_limit`
  in bodies and guard rejection; compiler literal and constant limits agree
  (27E).
- [x] GC/copying integration (28).

### F11 — Floating-point values and arithmetic

Plan: [23](11-plan.md#step-23), [28](11-plan.md#step-28),
[38](11-plan.md#step-38).

- [x] Finite binary64 values, operations, conversions and mixed comparisons.
- [x] GC/copying integration (28).
- [ ] Broader numeric scope when selected.

## Executable language semantics

### F12 — Equality, comparisons and term ordering

Plan: [31](11-plan.md#step-31), [32](11-plan.md#step-32),
[42](11-plan.md#step-42).

- [x] Exact/numeric equality and ordering for every admitted representation.
- [x] Extend as identities, callables and native records arrive (steps 31, 32, 42).

### F13 — Pattern matching and bindings

Plan: [9](11-plan.md#step-9), [13](11-plan.md#step-13), [16](11-plan.md#step-16),
[21](11-plan.md#step-21), [22](11-plan.md#step-22), [46](11-plan.md#step-46).
Contract: [patterns](../docs/patterns.md).

- [x] Function heads and body matches over the admitted domain.
- [x] `case` clauses with exported/unsafe bindings (step 9).
- [x] `if` clauses with the same export/unsafe rules (step 10).
- [x] `catch`/`try` and `maybe` binding contexts (steps 12, 13, 16).
- [x] List comprehension generators, filters and zip groups (step 21).
- [x] Binary and map generators and producers (step 22).
- [ ] Fun and receive contexts.

### F14 — Guards

Plan: [9](11-plan.md#step-9), [10](11-plan.md#step-10),
[52](11-plan.md#step-52). Contract: [guards](../docs/guards.md).

- [x] Audited catalog (77 of 81 rows) and grouped/strict/lazy clause guards.
- [ ] `self/0`, `node/0,1`, native `is_record/1` with F07/F17/F22/F26.
- [x] `case` clause guards (step 9) and `if` guards (step 10).
- [ ] Other guards outside function clauses; identity/function type tests.

### F15 — Multiple function clauses

- [x] Ordered clause selection, conservative joins, `function_clause`.

### F16 — Expression sequences and control flow

Plan: [9](11-plan.md#step-9), [10](11-plan.md#step-10), [16](11-plan.md#step-16),
[21](11-plan.md#step-21), [22](11-plan.md#step-22).

- [x] Sequences, body matches, strict/lazy boolean operators.
- [x] `begin`/`end` blocks and `case` with OTP comparisons (step 9).
- [x] `if` with OTP comparisons (step 10).
- [x] `maybe` with `?=`, `else` and `else_clause` (step 16).
- [x] List comprehensions with OTP comparisons (step 21).
- [x] Binary and map comprehensions with OTP comparisons (step 22).

### F17 — Record expansion and execution

Plan: [29](11-plan.md#step-29), [30](11-plan.md#step-30),
[31](11-plan.md#step-31).

- [x] Ordinary declarations, defaults, construction, access, matching, tests.
- [x] Record updates (step 29) and `record_info/2` (step 30).
- [x] Local native records: runtime cells and services (31B), compilation (31C).
- [x] Qualified/imported native records, `-export_record`, `-import_record` (31D).
- [x] Anonymous native records (31E).

### F18 — Closures and function values

Plan: [32](11-plan.md#step-32), [33](11-plan.md#step-33),
[34](11-plan.md#step-34).

- [x] Representation, arity and code ownership; `fun F/A`, `fun M:F/A`, calls
  of function values with `badfun`/`badarity`/`undef` (step 32).
- [x] Captures: rooted traced environments; anonymous fun construction and
  invocation lowering (step 33).
- [x] Verify captures after return/GC, wrong arity, copies, module lifetime
  (step 33: `executables_closures`, `runtime_funs`).
- [x] Named funs, recursion and tail recursion through the name (step 34:
  `executables_named_funs`).

### F19 — Dynamic calls

Plan: [35](11-plan.md#step-35).

- [x] Supported forms (`M:F(Args)`, `apply/2,3`, `fun M:F/A` with
  variables), arity checks and missing-function outcomes (step 35).
- [x] Generic lookup/invocation with module pins (export frames, modules stay
  registered); verify failures (step 35: `executables_dynamic_calls`,
  `runtime_funs`). Builtins of the bridge catalog are reached dynamically and
  as funs (step 36: `executables_builtin_bridge`).

### F20 — Erlang exceptions

Plan: [11](11-plan.md#step-11)–[15](11-plan.md#step-15). The checked failure
channel carries `function_clause`, service reasons and the three source classes.

- [x] `error/1,2,3`, `exit/1`, `throw/1` classes, any-term reasons and uncaught
  outcomes (step 11).
- [x] `catch Expr` values and binding safety (step 12).
- [x] `try ... of ... catch`: class/reason patterns and guards, re-raise,
  `try_clause`, binding safety (step 13).
- [x] Stack traces (bounded to 8 frames, documented OTP differences),
  `Class:Reason:Stack`, `erlang:raise/3`, `error/2,3` arguments (step 15).
- [x] `try ... after` on normal, caught and uncaught paths, after-body
  exceptions replacing the original, allocation faults (step 14).
- [ ] Nested handling and cleanup across module and builtin calls.

### F21 — Recursion and proper tail calls

Plan: [17](11-plan.md#step-17)–[20](11-plan.md#step-20), [34](11-plan.md#step-34).

- [x] Recursive call components in resolution and inference (step 18).
- [x] Frame/tail-call model compatible with roots, exceptions and suspension
  (compare explicit continuations with LLVM coroutines). Decided in step 17
  ([execution model](../docs/execution-model.md)), implemented in step 19:
  explicit frames, `musttail` transfers, local/mutual/remote tail calls.
- [x] Deep tail and non-tail recursion, local/remote/mutual (steps 19, 20):
  body recursion grows the process stack until the host refuses memory
  (`out_of_memory`, exit 70); a per-process stack cap is opt-in.
- [x] Recursion and constant-stack tail recursion through named funs (step 34).

## Processes and runtime services

### F22 — Cooperative process execution

Plan: [17](11-plan.md#step-17), [43](11-plan.md#step-43), [44](11-plan.md#step-44),
[48](11-plan.md#step-48), [49](11-plan.md#step-49). Contract:
[processes](../docs/processes.md).

- [x] Entry/return/yield/exit protocol with rooted continuations and bounded work
  (step 43: yields at function entries; builtins bounded in step 43A).
- [x] Create/start/resume/finish and spawn/1,3 on one thread (step 43).
- [x] Exit reasons and crash reports (step 44).
- [ ] Links and monitors (steps 48, 49).

### F23 — Scheduler workers and wakeups

Plan: [56](11-plan.md#step-56), [57](11-plan.md#step-57), [62B](11-plan.md#step-62b).

- [ ] Queue ownership, workers, reductions, wakeups, synchronized shutdown;
  verify fairness and lost-wakeup races.

### F24 — Signals and message sending

Plan: [45](11-plan.md#step-45).

- [x] Ordered signal inbox for every message (including self-send), F05 copying
  before mailbox insertion, rooted messages in transit; lower send (step 45).

### F25 — Selective receive and timeouts

Plan: [46](11-plan.md#step-46), [47](11-plan.md#step-47),
[57](11-plan.md#step-57), [62B](11-plan.md#step-62b) (timer wheel).

- [x] Rooted cursors, ordered pattern/guard selection, removal of only the match (step 46).
- [x] Arrival handshake, suspension and timeouts compared with OTP (step 47; timer wheel: 62B).

### F26 — Production builtin functions

Plan: [2](11-plan.md#step-2), [4](11-plan.md#step-4), [36](11-plan.md#step-36)–[40](11-plan.md#step-40),
[43A](11-plan.md#step-43a), [50](11-plan.md#step-50).

- [x] Guard catalog services; `erlang:display/1` and `erlang:halt/0,1`.
- [x] Generic production registration bridge (step 36): `BuiltinRegistry`
  by module/name/arity with transactional batches, the append-only
  `bridge_builtins` catalog, the bridge service (frames since 43A), builtin frames entered
  by dynamic calls and funs; guard BIFs, operators, `display`, `halt`, the
  raise family and `function_exported/3` registered.
- [x] Term access family (step 37): `setelement/3`, `make_tuple/2,3`,
  `tuple_to_list/1`, `list_to_tuple/1`, `++`/`--` operators and functions;
  `element`, `tuple_size`, `hd`, `tl`, `length`, `map_get`, `map_size`,
  `is_map_key` in bodies.
- [x] Conversion family (step 38): atoms, integers (bases 2..36), floats
  (`float_to_list/1,2` formats), binaries and iolists; atom-table limit.
- [x] Project-owned `lists`/`maps` subsets compiled with programs that name
  them (step 39, `library/stdlib`).
- [x] Console output (step 40): `io:format/1,2` (`~w ~p ~s ~c ~b ~B ~i ~n ~~`,
  widths, precisions, pads, `t`/`l`/`k`, OTP `~p` layout) and `io:put_chars/1`.
- [x] Interruptible long-running builtins (step 43A): bridge builtins are
  entered like functions (`erlang_aot_builtin_frame_v1`); `length/1` in bodies,
  `++`, `--`, `binary_to_list/1`, `list_to_binary/1` and `iolist_to_binary/1`
  run in portions with rooted state and yield between them.

### F27 — Typed/native callables and conversions

Plan: [41](11-plan.md#step-41).

- [x] Concrete use cases, checked conversions, generic fallback; no STL values
  or C++ exceptions across generated boundaries (step 41: typed builtin
  adapters, `runtime/src/builtins/typed.hpp`).

### F28 — Concurrent code-server access

Plan: [55](11-plan.md#step-55).

- [ ] Synchronized publication/lookup with pins through invocation; stress
  registration, lookup and teardown.

## Optimization and developer tooling

- **F34 — Precise type inference** ([58A](11-plan.md#step-58a)–[58G](11-plan.md#step-58g)):
  [ ] fact domain decision; [ ] literals; [ ] operators and builtins;
  [ ] containers; [ ] funs; [ ] local inputs from callers; [ ] pattern and
  guard narrowing. Expectations: `tests/fixtures/inference/values.erl` and
  `base_types.erl` (7 of 39 and 3 of 46 functions at their expected type on
  2026-10-08).
- **F29 — Source-driven specialization** ([59](11-plan.md#step-59)): [ ] remove
  real checks for new operations with generic fallback and existing caps.
- **F30 — Debug information** ([60](11-plan.md#step-60)): [ ] LLVM debug
  metadata mapped to Erlang source through macros/includes.
- **F31 — Profiling** ([61](11-plan.md#step-61)): [ ] bounded-overhead cost
  attribution with deliberate enablement.
- **F32 — Link-time optimization** ([62](11-plan.md#step-62)): [ ] LTO modes
  preserving descriptors, startup and exports.
- **F33 — Indexed code lookups** ([62A](11-plan.md#step-62a)): [ ] hash maps
  for module/function name lookups of dynamic calls and descriptor lookups,
  replacing the linear scans of `CodeServer`.

## Validation obligations

Keep historical results with their original hosts and revisions.

- **V01 — Native platform matrix** ([63](11-plan.md#step-63)–[66](11-plan.md#step-66)):
  [ ] Linux x86/x64/ARM/AArch64, macOS Apple Silicon, Windows x86 builds,
  quality and executable workflows at O0/O2.
- **V02 — Compiler/frontend sanitizers** ([67](11-plan.md#step-67),
  [68](11-plan.md#step-68)): [ ] ASan/UBSan/LSan with a compatible SDK, no
  suppressions.
- **V03 — Broader OTP compatibility** ([1](11-plan.md#step-1),
  [2](11-plan.md#step-2), [58](11-plan.md#step-58), [69](11-plan.md#step-69)):
  [x] pin refreshed and program fixtures goldened (steps 1–2); [ ] upstream
  Common Test suites and wider differential comparisons.
- **V04 — Remaining test migration** ([8](11-plan.md#step-8),
  [70](11-plan.md#step-70)): [x] executable golden runner (step 8); [ ] audit
  the [ledger](../docs/validation.md#test-design) and retire covered adapters.

## Optional or explicitly deferred scope

Each ends in a recorded selection, deferral or omission; omission is never
checked as implemented.

- **D01 — Dynamic modules and code upgrades** ([71](11-plan.md#step-71)):
  static-only, native dynamic libraries, or an upgrade model.
- **D02 — Atom collection** ([72](11-plan.md#step-72)): bounded permanent
  storage versus collection.
- **D03 — Behavior-changing attributes and transforms**
  ([73](11-plan.md#step-73)): compile options, parse transforms, on-load.
- **D04 — Public stage interchange** ([74](11-plan.md#step-74)): only for a
  concrete consumer.
- **D05 — Intermediate-stage readers** ([75](11-plan.md#step-75)): directories
  reserved only.
- **D06 — C/FFI interoperability** ([76](11-plan.md#step-76)): only for a
  concrete external caller.
- **D07 — Project schema extensions** ([77](11-plan.md#step-77)): defaults,
  profiles, dependencies, exclusions, fetching, watch/cache, parallel builds.

## References

[Completed work](00-finished.md), [architecture](arch.md), [file map](files.md),
[compilation](../docs/compile.md), [validation](../docs/validation.md),
[runtime](../docs/runtime.md), [runtime heap](../docs/runtime-heap.md),
[features](../docs/features.md); design sketches in `runtime/design/`.
