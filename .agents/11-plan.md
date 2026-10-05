# Remaining compiler and runtime work — implementation plan 11

Created 2026-10-03 from [completed work](00-finished.md) and
[the feature backlog](01-todo.md). Planning only: every step is unchecked and
creating this plan starts no implementation. It replaces the earlier 51-step
draft with smaller, single-commit steps.

## How to use this plan

- Steps run in numbered order by default. Each lists its backlog owners and
  dependencies; an independent step may move earlier once its dependencies
  pass.
- Each single step ends with one focused commit titled `[plan11] <full step title with step number>`. A step that
  grows beyond one reviewable change is split into lettered sub-steps (`12a`,
  `12b`) before coding, not widened silently.
- **Decision** steps publish a short contract in `docs/` (plus a prototype where
  stated) and enable no source feature by themselves.
- Check a success or test box only with recorded evidence. Update the backlog
  checkboxes for the slice delivered.
- Optional D-items (steps 71–77) end in a recorded selection, deferral or
  omission. An omitted item is never checked as implemented.
- Phase C (steps 8A–8I) was inserted on 2026-10-04 after phase B closed; its
  steps run before step 9 and keep their letter IDs in commit titles.

## Common gate and rules (apply to every step)

- **Gate:** freshly configure `build/debug` with compiler, runtime and
  `BUILD_TESTING=ON`; build; run fast-mode CTest (`ctest --preset debug-fast`);
  run `cmake --build build/debug --target check-quality` (changed files and
  header dependents). Run full-mode CTest (`ctest --preset debug -j <N>`; each
  test takes two slots, so N/2 run at once) and `check-quality-all` when a
  phase or major feature completes. Lizard and clang-tidy pass
  without raised thresholds or suppressions. Code is clang-formatted; new and
  changed `.erl`/terms files pass erlfmt.
- **Code:** project-internal C++23; document field and function intent in 1–2
  lines; keep cyclomatic complexity low; runtime stays LLVM-free.
- **OTP:** at the start of OTP-dependent work follow
  [otp-reference.md](../docs/otp-reference.md) to check `maint-29`. OTP sources
  and copies never enter git; commit only owned sources, observations and
  golden results. Normal builds and tests require neither OTP nor its checkout;
  goldens change only by explicit, reviewed regeneration.
- **Tests:** prefer real `.erl` sources through the CLI, linked executables and
  OTP-derived goldens. Where applicable cover positional and project drivers,
  O0/O2, specialization on/off and local/remote calls. Add focused unit tests
  only for budgets, ownership, invalid handles and injected faults that source
  cannot reach. Do not remove a useful test before equivalent behavioral
  coverage exists.
- **Representations:** a new term kind needs construction, comparison,
  printing, tracing, copying, destruction and error ownership rules before
  source code may produce it. Forged tagged words are never accepted as valid
  identities.
- **Failures:** keep legality, availability and runtime failure distinct.
  Invalid source is diagnosed even when unreachable; missing services stay
  explicit unavailable-capability diagnostics.
- **Docs:** update affected contracts, examples, `00-finished.md`, `arch.md`,
  `files.md` and `aimemory.md`. Unavailable hosts and tools are recorded as
  gaps, never as passes.

<a id="completed-patternmatch"></a>

## Retained context from the completed pattern/guard plan

Old plan 10 steps 1–20 and added 15a finished on 2026-10-03; their numbers are
historical and unrelated to the steps below. Per-step records are in
[the archive](00-finished.md#completed-patternmatch) and condensed in
[validation history](../docs/validation.md#history).

Executable baseline: ordered function clauses with guards, body
matches/sequences, acyclic local and exported remote calls. Admitted terms:
atoms/booleans, arbitrary integers, finite binary64 floats, tuples,
proper/improper lists, strings, exact-key maps, bitstrings and ordinary tuple
records (declarations, defaults, construction, access, patterns, tests).
Descriptors use ABI revision 4; the checked first-error channel is revision 2.

Invariants that later steps must keep:

- Candidate bindings publish only after head and guard success; `_` binds
  nothing; repeated names use exact equality; body matches evaluate the RHS once
  and chains right-to-left.
- Map keys read only incoming bindings; binary sizes also read preceding
  segments of the same binary.
- Guard legality comes from the pinned catalog and lint rules, not from runtime
  registration. Semantic guard errors reject the alternative; infrastructure
  errors stop execution.
- Every fallible call is checked before its output is used; errors are owned,
  first failure wins, outer cleanup runs, and retry in the same context works.
- Shape proofs dominate extraction; specs never authorize runtime access or
  narrow representation. Specialization limits: 3 variants per function, 32
  per module, 128 per target, 2x generic IR growth, with generic fallback.
- Failed compilation preserves earlier valid outputs; multi-file replacement is
  not atomic.

| Retained contract | Documents |
| --- | --- |
| Patterns, clauses, body matches | [Patterns](../docs/patterns.md) |
| Guards and catalog | [Guards](../docs/guards.md) |
| Bindings, types, inference facts | [Semantic analysis](../docs/semantic.md) |
| Failure channel, atoms, roots, registration | [ABI](../docs/abi.md), [terms](../docs/terms.md#atoms) |
| Representations | [Terms](../docs/terms.md), [runtime memory](../docs/runtime.md#process-memory) |
| Owned fixtures and history | [Validation](../docs/validation.md), [fixture instructions](../tests/fixtures/patternmatch/generated/README.md) |

Last reviewed `maint-29` pin: `21776803ecd11f5fa948732c0ec66b8f325dedfc`;
oracle OTP 29.1.1 / ERTS 17.1. Latest combined Windows x64 Debug gate: 126
CTests (123 fast) and 258 production quality units.

## Step overview

| Phase | Steps | Backlog owners |
| --- | --- | --- |
| A. Baseline and fixtures | [1](#step-1)–[2](#step-2) | V03 |
| B. Production executables | [3](#step-3)–[8](#step-8) | F01, F26, V04 |
| C. Classic process heap | [8A](#step-8a)–[8I](#step-8i) | F02–F05, F09 |
| D. Control flow and exceptions | [9](#step-9)–[16](#step-16) | F13, F14, F16, F20 |
| E. Execution model, recursion, comprehensions | [17](#step-17)–[22](#step-22) | F02, F13, F16, F21, F22 |
| F. Memory management | [23](#step-23)–[28](#step-28) | F02–F05, F08–F11 |
| G. Records, function values, dynamic calls | [29](#step-29)–[35](#step-35) | F03, F12, F14, F17–F19, F21 |
| H. Builtins and libraries | [36](#step-36)–[41](#step-41) | F26, F27 |
| I. Processes and messaging | [42](#step-42)–[53](#step-53) | F02, F04, F05, F07, F14, F22, F24–F26 |
| J. Multi-worker scheduling | [54](#step-54)–[57](#step-57) | F06, F23, F25, F28 |
| K. End-to-end projects | [58](#step-58) | F01, V03 |
| L. Optimization and tooling | [59](#step-59)–[62](#step-62) | F29–F32 |
| M. Validation closure | [63](#step-63)–[70](#step-70) | V01–V04 |
| N. Optional scope decisions | [71](#step-71)–[77](#step-77) | D01–D07 |
| O. Final closure | [78A](#step-78a), [78](#step-78) | all |

---

## Completed steps 1–8G (compact record)

Full step texts, criteria and per-step evidence are in Git history (last full
version at `a4e07bb`). Every step below passed the common gate.

### A. Baseline and fixtures

<a id="step-1"></a>

### 1. Refresh the OTP reference and record a fresh baseline

Done 2026-10-03. `maint-29` unchanged at `21776803`; fresh full gate 125/125,
258 quality units; grammar, corpus and source audits pass.

### 1A. Tests run time too long

Done 2026-10-03. `ERLANG_AOT_TEST_MODE=fast|full` (unset = full), presets
`debug-fast`/`windows-debug-fast`, `make test`/`make test-full`; label
`full_only`. Fast mode (O0 positional + O2-off project) about 60 s; full
`-j 16` about 85 s.

### 1B. Quality check checks too much

Done 2026-10-03. `check-quality` and `make format` cover files changed since
`HEAD` plus header dependents (`cmake/quality_scope.py`, Ninja deps);
`check-quality-all`/`format-all` scan everything.

### 1C. Run the available documentation check

Done 2026-10-03. `docs/` reduced to 15 brief current-state notes indexed by
`docs/README.md`; originals at `2777c98`.

<a id="step-2"></a>

### 2. Author target program fixtures and their feature map

Done 2026-10-03. Six OTP-goldened programs in `tests/fixtures/programs/`
(`textstats`, `frames`, `avltree`, `ring`, `kvstore`, `supervise`) with a
feature map README; CTest `programs_compile`, opt-in `programs_oracle`.

### B. Production executables

<a id="step-3"></a>

### 3. Decide the entrypoint, arguments and exit-status contract

Done 2026-10-03. `docs/executables.md`: `--entry MODULE[:FUNCTION]` or manifest
`entry`; `main/1` gets argv strings; exit 0 on return/`halt()`, `halt(N)` = N,
escaping exception 1, runtime failure 70. `driver/entry.cpp`; CTest
`linking_entry`.

### 3A. Add escript compile mode

Done 2026-10-03. A `#!` first line selects escript rules (`driver/escript`,
`semantic/escript`): `main/1` required, `-mode` validated, uncaught exception
exits 127. CTest `linking_escript`.

<a id="step-4"></a>

### 4. Add runtime term printing and `erlang:display/1`

Done 2026-10-03. `format_term` (`runtime/src/terms/term_text*.cpp`) prints `~w`
and display text iteratively under a 64 MiB cap; `erlang:display/1` lowers to
`erlang_aot_display_v1` (status `output_failure`). Maps print in key order (OTP
order is not reproducible). Goldens `tests/fixtures/printing/` (9,542 values);
CTests `runtime_printing`, `printing_display`.

<a id="step-5"></a>

### 5. Generate the startup object

Done 2026-10-04. An explicit entry adds startup module `codegen/startup`
(`eav1_start`) whose `main` calls `erlang_aot_main_v1` with an
`abi::v1::StartupDescriptor`; ordered teardown on every path.
`erlang:halt/0,1` is a body builtin (`erlang_aot_halt_v1`). CTests
`linking_startup`, `runtime_startup`.

<a id="step-6"></a>

### 6. Link executables from positional CLI inputs

Done 2026-10-04. `compiler/src/linking/`: `-o` stages objects privately and
links with `clang --driver-mode=g++ --target=<triple>` plus the runtime
archive, then replaces the output (`.exe` added on Windows). Options
`--linker`, `--runtime-library`; archive members checked against the target.
CTest `linking_executable`.

### 6A. Link a single project target with explicit `-o`

Done 2026-10-04. `--project` with `-o` links its single target; executable
linking is no longer `notimpl`.

<a id="step-7"></a>

### 7. Link executables from project targets

Done 2026-10-04. Targets requesting an executable (manifest `output`/`entry`
or CLI `-o`/`--entry`) link to `-o`, else `output`, else
`<manifest-dir>/build/<target>`; outputs publish only after every target
succeeds. CTest `linking_project`.

<a id="step-8"></a>

### 8. Add the executable golden test runner

Done 2026-10-04. Case = `tests/fixtures/executables/<case>/` sources plus
`golden.json` (authored entry/args/stderr regex, OTP stdout/exit status);
`tests/compiler/executables/{run,regenerate}.py`, CTests `executables_<case>`
and `executables_selfcheck`; policies from `matrix.py`. Phase B close: fast 135,
full 138/138, `check-quality-all` 272 units.

### C. Classic process heap

Inserted 2026-10-04. The phase-B heap (non-moving chunk list, per-cell
`std::map` index, fixed 64-byte bitstring cells with a destructor registry,
`shared_ptr`-pinning host terms, per-frame root buffers, no overflow area)
could not grow into BEAM-style collection. Target design, contract in
`docs/runtime-heap.md`:

- A heap is a flat word array: boxed objects start with a header (tag `00`,
  kind, word count, untraced payload counted), cons cells are two headerless
  words, so any area parses left to right.
- Each process owns one heap block plus fragments for allocation that may not
  move the heap, and a separate root stack; no old heap yet.
- Binaries over 64 bytes are shared `std::shared_ptr` buffers outside every
  heap, referenced by `refc_binary` cells on a per-process off-heap list.
- Steps 8A–8I keep generated-code ABI and every golden unchanged. Until step
  26 the heap moves only at explicit host safe points; generated-code overflow
  goes to fragments.

<a id="step-8a"></a>

### 8A. Decide the classic process heap contract

Done 2026-10-04. `docs/runtime-heap.md`: header word (5 kind bits, count from
bit 7), per-kind cell table (map count in words), filler, areas, ERTS sizing
from 233 words, one `limit_bytes` budget including off-heap buffers, admission,
safe points. Full-only CTest `runtime_heap_measurements`; baseline `bb09359`:
kernel 264/81 ms, 24 MB index, 66 KB per context.

<a id="step-8b"></a>

### 8B. Split binary cells and add the off-heap list

Done 2026-10-04. `HeapBinaryCell` (at most 64 bytes inline) and
`RefcBinaryCell` (6 words: offset, bits, `shared_ptr<const BinaryBuffer>`,
`next_`); `memory/off_heap` links after publication, relocates by move
construction and releases at teardown. `HeapDestructor` removed; buffers are
charged via `off_heap_words`. CTest `runtime_off_heap`.

<a id="step-8c"></a>

### 8C. Make heap areas parseable and add a heap walker

Done 2026-10-04. `memory/heap_walk` (`parse_cell`, `walk`) and
`ProcessHeap::verify()` (`memory/heap_verify`, `HeapCensus` or
`corrupt_heap`); `BoxedKind::filler`. CTest `runtime_heap_walk`; failure
injection verifies the heap after each rollback.

<a id="step-8d"></a>

### 8D. Admit heap words by header instead of the object index

Done 2026-10-04. Object index removed; admission = word-aligned address below
an area top plus header/cons shape matching the tag. No start bitmap: process
pointers always name object starts. `TermAccess::object` decodes from the
header. CTest `runtime_admission`; side bytes 24 MB to 3.4 KB.

<a id="step-8e"></a>

### 8E. Hold host terms as raw words between safe points

Done 2026-10-04. `Term` = word + borrowed `HeapStorage *` + weak lifetime +
collection count (`expired_context`, `stale_term`); no storage pin. Result
handoffs (`std::optional<Word>`) and the error payload are process roots;
`ProcessContext::visit_roots` covers stack, handoffs, payload and the explicit
span of `collect(span<Word>)` (still `not_implemented`). CTest `runtime_roots`
case `root_set`.

<a id="step-8f"></a>

### 8F. Move generated root frames onto a process stack

Done 2026-10-04. `GeneratedRoots` is a minimal segmented stack: one-page
segments (4096 bytes including a two-pointer allocator header; 507 slots on
64-bit), larger frames take whole pages, the top segment is freed when empty,
frames never move. ABI, 1,000,000-word and 4,096-frame limits unchanged. A
flat moving stack waits for frame-base reloads (steps 17, 24, 26).

<a id="step-8g"></a>

### 8G. Replace chunks with a contiguous heap and heap fragments

Done 2026-10-04. `HeapStorage` owns one process's single heap block `heap_`,
created by the first reservation at `max(min_heap_words, request)`, and its
fragment chain `fragments_` (newest fragment tried after the heap, else a new
one of at least `min_heap_words`, capped by the budget). `HeapOptions` is
`{min_heap_words = 233, limit_bytes}`; `reserve` has no alignment parameter.
Rollback resets tops and drops a new fragment or heap block. CTest
`runtime_heap_fragments`; 1,000 contexts take 2.4 KB and 233 words each; the
100k kernel spans about 3,000 fragments until 8H.

## C. Classic process heap (remaining)

<a id="step-8h"></a>

### 8H. Collect on explicit host request with a copying collector

Backlog: F03, F04. Depends on: [8G](#step-8g). Absorbs the former step 25.

Full-sweep Cheney copy: roots are stack frames, process root words (result
handoffs, error payload) and the caller's explicit root span; live objects from
the heap and fragments move into a new block
sized by the growth policy, leaving forwarding headers; roots are rewritten;
the off-heap list is swept and fragments are freed. Allowed only at a safe
point: until step 26, when the context is not running generated code.

- Success criteria
  - [x] Rooted values stay valid and equal, internal sharing is preserved,
    unreachable cells are reclaimed and last-owner binaries are released once;
    the heap grows or shrinks per policy. Host `Term`s taken before the
    collection report the stale-term error.
  - [x] A request at an unsafe point returns `unsafe_point` with no change;
    failure to allocate the new block leaves the heap untouched.
  - [x] `ProcessHeap::collect` leaves the deferred-services table; statistics
    report heap, fragment, stack and off-heap sizes.
- Tests
  - [x] Focused runtime tests: explicit roots across repeated collections, nested
    and shared graphs of every layout, binary release counts, stale host terms,
    verifier after each collection.
  - [x] Injected failure of the new-block allocation.
- Evidence (2026-10-04): `memory/heap_collect` (`Copier`, `heap_size_at_least`),
  off-heap sweep in `memory/off_heap`; CTest `runtime_collection`, new-block OOM
  in `runtime_lifecycle_failure`; tests that used `collect` as a deferred
  service now use deferred send / heap reservation faults. Fresh Windows x64
  Debug gate: fast CTest 140/140, `check-quality` 276 tidy units plus Lizard
  pass. Logs `build/plan11-step8h/`.

<a id="step-8i"></a>

### 8I. Close the heap rework

Backlog: F03, F04. Depends on: [8H](#step-8h).

- Success criteria
  - [x] `docs/runtime.md`, `docs/terms.md`, the ABI root-scope notes,
    `arch.md` and `files.md` describe the new heap; no reference to the chunk
    list, object index or cell destructors remains.
  - [x] The 8A kernel and footprint are measured again and compared in
    `docs/runtime-heap.md` (descriptive, not gated).
- Tests
  - [x] Fresh full-mode CTest and `check-quality-all` pass; counts recorded in
    `docs/validation.md`.
- Evidence (2026-10-04): stale chunk/index/pin wording also removed from
  `base_types.hpp` and `runtime/design/{processes,terms}.md`;
  `runtime_heap_measurements` now also collects the kernel list (700,000 live
  words in about 56 ms into a 999,631-word block; walk 173 to 72 ms; side bytes
  163,878 to 0). Fresh Windows x64 Debug full `-j 16`: 143/144, the
  `codegen_dependency` timeout under load passed alone (29 s); Lizard-all 0
  warnings; tidy-all 276 units passed with one job. Logs `build/plan11-step8i/`.

## D. Control flow and exceptions

<a id="step-9"></a>

### 9. Lower `begin`/`end` blocks and `case` expressions

Backlog: F13, F14, F16. Depends on: [8I](#step-8i).

Reuse the clause/guard matcher for `case` clauses; implement branch-variable
export (bound in every clause) and unsafe-variable diagnostics.

- Success criteria
  - [x] Clause order, guards, nested `case`, exported and unsafe variables
    match OTP; no matching clause raises `{case_clause, Value}`.
  - [x] Inference joins branch facts conservatively.
- Tests
  - [x] Golden programs for selection, fallthrough, nested cases, exported
    variables and `case_clause`.
  - [x] CLI diagnostics for unsafe and unbound variables matching OTP lint
    wording/classes.
- Evidence (2026-10-04): `maint-29` unchanged at `21776803`. Binding analysis
  (`binding_expressions`) schedules case clauses iteratively, reuses one
  identity per name across clauses and records exports in `Function::exports`;
  the walker lowers clauses with the one-input body plan and joins value and
  exports through `SSAUpdater`; `ErrorReason::case_clause = 9` (payload).
  OTP goldens `executables_case_select` (16 classify rows, records, maps,
  binaries, nested and remote cases, `case_clause`, `function_clause`) and
  `executables_case_scope` (exports, scrutinee bindings, nested exports,
  begin/end); bindings corpus +8 `case_*` rows regenerated with OTP 29.1.1
  (`unsafe_var`/`unbound_var` classes; wording stays `unsafe/unbound variable
  X`); `sibling_local` now compiles; `--print-types` join checks in
  `codegen_types`. Also fixed: body-match planning after a failed binding pass
  printed `invalid map<K, T> key`. Fresh Windows x64 Debug: fast CTest
  142/142; affected tests in full mode 7/7; Lizard 0 warnings; tidy 114
  changed units pass with one job (two-job run exited silently). Logs
  `build/plan11-step9/`.

<a id="step-10"></a>

### 10. Lower `if` expressions

Backlog: F14, F16. Depends on: [9](#step-9).

- Success criteria
  - [x] Guard-only clauses select in order; no true guard raises `if_clause`;
    variable export follows step 9 rules.
- Tests
  - [x] Golden programs for guard sequences, failing guards, `true` fallback and
    `if_clause`.
- Evidence (2026-10-05): `maint-29` unchanged at `21776803`.
  `semantic::branch_clauses` presents case and if clauses uniformly (optional
  pattern/guard, body), so binding analysis, guard analysis, capability
  planning, atom collection and the lowering walker reuse the step-9 paths;
  inference joins `if` results like `case`. `ErrorReason::if_clause = 10`
  (atom only; runtime `plain_reason`). OTP golden `executables_if_select`
  (`;`/`,` sequences, `andalso`/`orelse`, raising guards, `true` fallback,
  exports, nested case/if, `if_clause`, body error not retried); bindings
  corpus +4 `if_*` rows verified by OTP 29.1.1 (`--check` reproduces);
  `--print-types` `if` join checks; semantic `if_guard_call`/`if_in_guard`;
  placeholder `[guards]` sample now `self()` in a guard; avltree/textstats
  compile diagnostics lose their `if` rows. Fresh Windows x64 Debug: fast
  CTest 143/143; affected tests in full mode 8/8; Lizard 0 warnings; tidy 114
  changed units pass. Logs `build/plan11-step10/`.

<a id="step-11"></a>

### 11. Raise exceptions from source

Backlog: F20. Depends on: [9](#step-9).

Implement `erlang:error/1,2,3`, `throw/1` and `exit/1` through the checked error
channel with class and owned reason.

- Success criteria
  - [x] All three classes propagate through local/remote calls unchanged.
  - [x] Uncaught exceptions reach startup and print the step-3 report.
- Tests
  - [x] Golden programs raising each class at different call depths; stderr
    report and exit status checked.
- Evidence (2026-10-05): `maint-29` unchanged at `21776803`. `error/1,2,3`,
  `exit/1`, `throw/1` are body builtins (`ImmediateOperation::raise`),
  `erlang:`-qualified or auto-imported unless a local definition or
  `no_auto_import` shadows them (`semantic::body_builtin` in `pattern_calls`);
  `lower_raise` calls the existing `erlang_aot_raise_v2` with new
  `ErrorReason::raised_error/exit/throw` (11-13) whose payload is the whole
  reason (no new symbol or ABI revision); `error/2,3` extra arguments are
  evaluated and dropped until step 15. Startup prints
  `uncaught exception <class>: <reason>` (escript `escript: exception
  <class>: ...`). OTP golden `executables_raise_classes` (15 runs: each class
  qualified/unqualified, error/2,3, any-term reasons incl. bignum/map/binary,
  `exit(normal)`, remote depth 3, local depth 3, argument order, case body,
  `no_auto_import` shadowing); OTP's own class/reason output agrees for all 14
  raising runs. `linking_startup` adds an escript `throw` run. Fresh Windows
  x64 Debug: fast CTest 144/144; affected tests in full mode 22/22; Lizard
  and tidy (124 changed units) pass. Logs `build/plan11-step11/`.

<a id="step-12"></a>

### 12. Lower `catch Expr`

Backlog: F20. Depends on: [11](#step-11).

- Success criteria
  - [x] Values follow OTP: thrown value, `{'EXIT', Reason}` for exit, and
    `{'EXIT', {Reason, Stack}}` for error (stack per step 15; placeholder
    documented until then).
  - [x] Bindings inside `catch` follow OTP safety rules.
- Tests
  - [x] Golden programs for each class, nested catches and catch of runtime
    errors (`badmatch`, `function_clause`, `badarith`).
- Evidence (2026-10-05): `maint-29` unchanged at `21776803`. The walker sets
  `ExpressionLowering::handler` while the protected expression lowers (fresh
  badarg/badarith exits), so failure checks and `raise_reason` branch to it;
  the handler calls new `erlang_aot_catch_v1` (`runtime/src/process/exceptions`)
  which writes the catch value to a root slot and clears the channel; halts and
  runtime failures stay pending and continue outward. Stack placeholder `[]`
  (`docs/abi.md`). Binding analysis treats `catch` like a conditional scope
  (inner names unsafe); `binding_children` gained the missing catch child.
  `-compile` now admits warning-only `nowarn_*` options (OTP 29 warns
  `deprecated_catch`). OTP golden `executables_catch_values` (7 runs: classes,
  11 runtime reasons, nested, flow/exports/remote depth, uncaught after catch,
  `halt` not caught); bindings corpus +4 `catch_*` rows verified by OTP 29.1.1
  (`--check` reproduces); semantic `catch_expr`/`catch_in_guard`/
  `catch_unsafe`/`compile_nowarn`; mangling for `Catch`. Fresh Windows x64
  Debug: fast CTest 145/145; affected tests in full mode 25/25; Lizard 0
  warnings; tidy 277 units pass. Logs `build/plan11-step12/`.

<a id="step-13"></a>

### 13. Lower `try … of … catch`

Backlog: F13, F20. Depends on: [11](#step-11).

- Success criteria
  - [x] Class/reason patterns and guards select handlers in order; unmatched
    exceptions re-raise unchanged; `of` clauses fail with `try_clause`.
  - [x] Exceptions inside `of` clauses and handlers are not caught by the same
    `try`.
- Tests
  - [x] Golden programs for each class, default `throw` class, nested tries,
    re-raise and `try_clause`.
- Evidence (2026-10-05): `maint-29` unchanged at `21776803`. Catch-clause class
  and stacktrace are now AST expression nodes (bindings anchor on them; tree
  and dump output unchanged). `semantic::branch_clauses` lists a try's `of`
  then catch clauses (`Branch::handler`, `first_handler`), so guard analysis,
  pattern planning and atoms reuse the case paths. Binding analysis: body and
  clauses form one conditional scope (all names unsafe afterwards), `of`
  clauses see body names, catch clauses see them unsafe (OTP `Uvt`). The
  walker protects only the body; its handler calls new
  `erlang_aot_exception_v1` (class atom + reason, clears the channel), catch
  clauses match class (omitted = `throw`), reason and guard from the pre-try
  bindings, and no match calls new `erlang_aot_reraise_v1`; `of` exhaustion
  raises `ErrorReason::try_clause = 14`. `after` and named stacktrace
  variables stay `[exceptions]` capabilities (steps 14/15). OTP golden
  `executables_try_catch` (11 runs: classes, default class, class variable,
  ten runtime reasons, ordered reason patterns of every kind, bound and
  repeated variables, raising guards, `of` selection, `try_clause`, `of` and
  handler exceptions escaping, nested and remote re-raise, flow, uncaught
  error/throw/exit/`try_clause`, `halt` not caught); bindings corpus +6
  `try_*` rows verified by OTP 29.1.1 (`--check` reproduces); semantic
  `try_expr`/`try_unsafe`/`try_in_guard`/`try_after`/`try_stacktrace`;
  placeholder sample now a named stacktrace; program compile goldens lose
  their `[exceptions]` rows; mangling for `Exception`/`Reraise`. Fresh Windows
  x64 Debug: fast CTest 146/146 (after the program-golden update); Lizard 0
  warnings after splitting `Walk::visit`; tidy 189 changed units pass. Logs
  `build/plan11-step13/`.

<a id="step-14"></a>

### 14. Lower `try … after`

Backlog: F20. Depends on: [13](#step-13).

- Success criteria
  - [x] `after` runs exactly once on normal return, caught and uncaught
    exceptions; its value is discarded; an exception inside `after` replaces
    the original.
  - [x] Rooted temporaries are released on every path.
- Tests
  - [x] Golden programs observing `after` execution order with
    `erlang:display/1` on each path.
  - [x] Allocation fault injected inside `after` keeps cleanup and ownership
    correct.
- Evidence (2026-10-05): `maint-29` unchanged at `21776803`. The walker opens
  a second `ProtectedScope` (`afters`) around the body and all `of`/catch
  clauses. After the clause join it lowers the after body on the normal path
  (the rooted try value kept in `AfterPath`); if anything protected can raise,
  it lowers the after body again from the after handler between
  `erlang_aot_exception_v1` and `erlang_aot_reraise_v1`. After-body failures
  leave through the enclosing handler (replacing the original); halts and
  infrastructure failures skip it; root slots are frame-owned and released at
  the function exit. No new runtime service. OTP golden `executables_try_after`
  (8 runs: normal/of/catch paths, discarded value, caught and uncaught
  exceptions, `try_clause`, of/handler raises, remote depth, after-body
  throw/exit/badmatch replacing the original, `catch` inside after, nested
  afters, bindings before the try, remote `after`, uncaught at top, `halt`
  skipping after). Native `codegen_after_fault_O0/O2`: a 256 KiB process budget
  makes the after body's binary fail with `resource_limit` on the normal and
  raising paths; the root stack is empty, the heap verifies, the channel is
  clear and the same context keeps working, twice over. Bindings corpus +3
  `try_after*` rows verified by OTP 29.1.1; semantic `try_after`/
  `try_after_unsafe`. Fresh Windows x64 Debug: fast CTest 149/149; Lizard 0
  warnings; tidy changed units pass after an optional-access fix. Logs
  `build/plan11-step14/`.

<a id="step-15"></a>

### 15. Provide stack traces and `erlang:raise/3`

Backlog: F20. Depends on: [13](#step-13).

Choose the stack-trace content (at least `{M, F, Arity, []}` frames; source
locations optional) and bind it in `Class:Reason:Stack`.

- Success criteria
  - [x] Stack terms are well-formed and bounded in depth; documented
    differences from OTP are explicit.
  - [x] `erlang:raise/3` re-raises with a supplied stack;
    `erlang:get_stacktrace` stays rejected as in OTP 29.
- Tests
  - [x] Golden programs comparing the top frames' module/function/arity with
    OTP.
  - [x] Malformed stack argument to `raise/3` behaves as OTP.
- Evidence (2026-10-05): `maint-29` unchanged at `21776803`. Root frames name
  their function: `erlang_aot_roots_enter_v5(context, count, frame)` takes a
  private `abi::v1::FrameDescriptor` (module descriptor, module and function
  name atom slots, arity; module atom tables now list both names).
  `GeneratedCallState::fail` copies the innermost 8 named frames
  (`GeneratedRoots::trace`) into `CallFailure::trace` for Erlang exceptions;
  `[{M, F, Arity, []}]` is built only by `catch`, handlers and reports.
  `erlang_aot_exception_v2` adds the stack slot (`Class:Reason:Stack` binds
  it), `erlang_aot_reraise_v2` keeps it on re-raise and implements
  `erlang:raise/3` (BEAM `raise_3` validation, `[]` added to `{M, F, A}`,
  cut to 8, invalid class or stack evaluates to `badarg`; `{Fun, Args}`
  entries wait for step 32), `erlang_aot_error_v1` keeps the `error/2,3`
  argument list for the top frame. Argument list and given stack are process
  roots. Lint: `stacktrace_bound`, `stacktrace_guard`, OTP 29 "removed" text
  for `erlang:get_stacktrace/0`; the `exceptions` catalog entry is
  implemented. Documented differences (docs/abi.md#stack-traces): `[]`
  locations, arity in `function_clause` frames, no BIF or below-entry frames,
  tail-call frames kept until step 19 (OTP also tail-calls functions that
  never return). OTP golden `executables_stack_traces` (9 runs: classes,
  `catch`, remote frames, typed runtime errors, depth 8, `error/2,3`
  arguments, re-raise through unmatched clauses, `after` and `raise/3`,
  unchanged re-raised stacks, valid and ten malformed `raise/3` stacks,
  uncaught `raise/3`); OTP's own output matched ErlangAoT's before the golden
  was written. Bindings corpus +3 `try_stack*` rows verified by OTP 29.1.1
  (`--check` reproduces); semantic `try_stacktrace`, `stack_bound`,
  `stack_in_pattern`, `stack_in_guard`, `raise_stack`, `raise_unqualified`,
  `get_stacktrace`; mangling for the four new spellings; `runtime_roots`
  enumerates the new roots. Fresh Windows x64 Debug: fast CTest 150/150;
  Lizard 0 warnings; tidy 129 changed units pass after splitting
  `spellings`. Logs `build/plan11-step15/`.

<a id="step-16"></a>

### 16. Lower `maybe` expressions

Backlog: F13, F16. Depends on: [9](#step-9).

- Success criteria
  - [x] `?=` short-circuits on mismatch, `else` clauses select on the value,
    and no matching `else` raises `{else_clause, Value}`.
  - [x] Feature enablement follows the preprocessor feature settings.
- Tests
  - [x] Golden programs for success, early exit, `else` selection and
    `else_clause`; feature-disabled source is rejected.
- Evidence (2026-10-05): `maint-29` unchanged at `21776803`. The AST keeps
  `MaybeMatch` items; `semantic::maybe_operands` lists the body values and
  `branch_clauses` the `else` clauses (`first_handler` 0), so guard analysis,
  pattern planning, atoms and the walker reuse the case paths. Binding
  analysis treats the body and `else` clauses as one conditional scope like a
  try (each `?=` binds after its value; `else` sees body names unsafe; nothing
  is exported). The walker's `MaybeScope` sends each failed `?=` to
  `maybe.else` with its rooted value; an `SSAUpdater` merge of those values is
  the result without `else`, or feeds a `CaseJoin` whose clauses raise
  `ErrorReason::else_clause = 15` (payload) when none matches. No new runtime
  service. Feature gating is the existing preprocessor keyword switch
  (`-feature(maybe_expr, disable)` or `--disable-feature` makes `maybe` an
  atom). The `pattern matching` catalog entry is implemented (its match-plan
  fallback is unreachable from source). OTP golden `executables_maybe_else`
  (6 runs: success, last `?=` value, early exit skipping later expressions,
  guarded `else` selection, pre-maybe bindings in `else`, nested and
  case-embedded maybes, caught and uncaught `else_clause`, exceptions from
  `else`, badmatch inside a body, remote calls). Bindings corpus +4 `maybe_*`
  rows verified by OTP 29.1.1 (`--check` reproduces); semantic `maybe_expr`,
  `maybe_else`, `maybe_unsafe`, `maybe_else_unsafe`, `maybe_in_guard`,
  `maybe_disabled`. Fresh Windows x64 Debug: fast CTest 151/151; Lizard 0
  warnings; tidy 98 changed units pass after replacing a direct PHI (analyzer
  false positive inside LLVM) with `SSAUpdater`. Phase D close: full `-j 16`
  155/155 (259 s), `check-quality-all` 277 units and Lizard pass. Logs
  `build/plan11-step16/`.

## E. Execution model, recursion, comprehensions

<a id="step-17"></a>

### 17. Decide the frame and continuation model

Backlog: F02, F21, F22. Depends on: [11](#step-11). **Decision.**

Compare explicit heap frames, LLVM coroutines and CPS-style lowering with a
small compiled prototype that calls, recurses, yields and resumes.

- Success criteria
  - [x] `docs/execution-model.md` defines call, return, tail call, yield,
    resume, exit, exception propagation and root visibility for each frame.
  - [x] It decides the successor of the interim 8F segmented root stack: a
    flat per-process stack with in-stack frame headers and base-plus-offset
    slot addressing, or the frame storage the chosen model needs instead.
  - [x] The choice works on all required targets (including Windows x86 and
    32-bit ARM) or names the fallback per target.
- Tests
  - [x] The prototype runs return, deep recursion, yield/resume and error exit
    on the host, recorded in the decision document.
  - [x] IR inspection on both word widths for the chosen lowering.
- Evidence (2026-10-05): not OTP-dependent (no `maint-29` check). Decision:
  explicit frames on one flat, moving per-process stack (4-word header:
  `previous` offset, descriptor, `resume` and `handler` continuation indices;
  base-plus-offset slots), arguments/results in process X registers, an entry
  plus one body per function that switches on the resume index, and only
  `musttail` transfers of `void (Process *)` code, so native depth is
  constant; yield = entry reduction count reaching zero; exceptions unwind to
  the innermost handler frame; trampoline fallback with the same frames.
  Prototype `tests/prototypes/execution_model/` (`model.hpp`, hand-lowered
  `generated.cpp`, `runtime.cpp` scheduler; rejected `native.cpp` and C++20
  `coroutines.cpp`; `run.py`, not a CTest): Windows x64 host, clang 23.1.2,
  O0 and O2 PASS for both transfer forms (return, 1M-deep recursion, 10M tail
  calls, caught and uncaught error at depth 100k with an 8-frame trace, two
  interleaved processes in 1,002 slices); `generated.cpp` compiles for x86_64
  and i686 Windows/Linux, AArch64 Linux, ARMv7 Linux and arm64 macOS at O0
  and O2 with every transfer `musttail` (15 at O0, 14 at O2) and tail jumps
  in the assembly; IR on both word widths recorded. Native calls need 80 B
  native stack per level and cannot yield; coroutines allocate per call
  (64-80 B), lack tail calls and run 3.4x slower. No production code changed,
  so the gate was not rerun. Log `build/plan11-step17/prototype.log`.

<a id="step-18"></a>

### 18. Accept recursive call graphs in analysis

Backlog: F21. Depends on: [17](#step-17).

Remove the acyclic-batch restriction: resolve recursive and mutually recursive
components and run bounded inference over them.

- Success criteria
  - [x] Self, mutual and cross-module recursion compile; inference terminates
    with widening and stays sound.
  - [x] Cycle-rejection diagnostics are removed only where lowering supports the
    case.
- Tests
  - [x] `--print-types` goldens for recursive functions.
  - [x] Golden programs for factorial, mutual even/odd and cross-module
    recursion with small depths.
- Evidence (2026-10-05): `maint-29` fetched, unchanged at `21776803`.
  `resolve_calls` keeps every call edge and orders strongly connected
  components callees-first (iterative Tarjan, `CallGraph::components`,
  `Component::recursive`); the Kahn cycle rejection is gone. Inference runs a
  non-recursive component once; a recursive one starts every member at
  `none()`, re-infers all members per round (each result joined with the
  previous; a pending recursive call adds nothing to a join), discards the
  previous round's expression facts, and after 16 rounds without convergence
  widens every member to `term()` (flagged widened) and recomputes facts once.
  Lowering already declared all functions before defining them, so recursion
  runs as native calls (depth bounded by the native stack until steps 19-20).
  Catalog `recursive calls` (ID 9) implemented, step 18, test
  `executables_recursion`. Tests: `codegen_types` (zero → 0, accumulator keeps
  `argument[0]`, swap → term(), never-returning → `none()`, mutual even/odd →
  `union(0, 1)`, 15-function ring converges, 16-function ring widens);
  semantic `local_cycle`/`self_cycle`/cross-module cycle now compile; the
  placeholder and `later_cycle` rejection cases are removed. OTP golden
  `executables_recursion` (6 runs: factorial to 25! bignum, tail accumulator,
  even/odd, list build/sum/reverse/length/zip, cross-module ping/pong, nested
  tuple depth, error unwinding through recursion caught and uncaught,
  `function_clause` from a guarded recursive clause), full matrix 48/48.
  Fresh Windows x64 Debug: fast CTest 152/152; affected tests full mode 13/13;
  Lizard 0 warnings; tidy 91 changed units pass. Logs `build/plan11-step18/`.

<a id="step-19"></a>

### 19. Implement proper tail calls

Backlog: F21. Depends on: [18](#step-18).

- Success criteria
  - [x] Local, mutual and remote tail calls run in constant native stack.
- Tests
  - [x] Golden programs looping 10 million iterations (local, mutual, remote) at
    O0 and O2. Reduced on user request to just past the stack budget: 2,000,000
    iterations (500,000 through case/if/begin clause bodies).
  - [x] IR inspection shows the chosen tail-transfer form.
- Evidence (2026-10-05): `maint-29` fetched, unchanged at `21776803`. The step-17
  model is implemented as a post-pass: lowering still emits native form
  (`erlang-arity` attribute, `erlang_aot.frame` slot marker, tail calls as
  `ret call` from a syntactic tail-position set through blocks, `case` and `if`
  clause bodies), and `codegen/frames` (`lower_frames`, run by the backend before
  IR inspection and by `optimize`) moves each function into `<sym>.body`
  (`void(ctx)`), reads the frame header and registers in a prologue that
  switches on the resume word, splits blocks after non-tail calls, spills values
  read after a call (the term slot already holding them, else raw slots via
  `DemoteRegToStack`), hoists constant addresses, and leaves only by `musttail`
  calls of the code `erlang_aot_enter_v1`/`tail_v1`/`return_v1` return.
  Descriptors `<sym>.frame` (7 words); exported symbols are host entries over
  `erlang_aot_invoke_v1`. Runtime `ProcessStack` (`process/stack`) replaces the
  segmented root stack: one `std::vector<Word>`, 4-word headers linked by
  offsets, 256 registers, 2^24-word budget, bottom frame per invocation.
  Exceptions still return through callers (channel check); no yield until step
  43. ABI version 5. Tests: OTP golden `executables_tail_calls` (local, mutual,
  remote, branches; full matrix O0/O2 x specialization x drivers);
  `codegen_cross_targets` requires `musttail call void` right after each
  enter/tail/return service on 7 targets (32/64-bit) at O0/O2/Os;
  `runtime_stack` (invoke, nested call, 20-step tail chain under a 32-word
  budget, budget failure, native exception, traces, root set); mangling of the
  6 new services checked against Clang. Removed root-stack services and tests
  (`roots_enter/leave`, segment and handoff checks, `service_consumer`
  `root_failures`). `codegen_measurements` IR-text bound raised to 2 MiB (client
  corpus: a resume block per call; objects stay under 1 MiB). Fresh Windows x64
  Debug (clang-cl; a GNU-clang++ configure lacks the UTF-8 manifest): fast CTest
  154/154, full 158/158 (both include the step-20 case); Lizard 0 warnings;
  tidy 278 units pass. Logs `build/plan11-step19/`.

<a id="step-20"></a>

### 20. Support deep non-tail recursion

Backlog: F21. Depends on: [19](#step-19).

- Success criteria
  - [x] Body recursion deeper than the native stack (for example building a
    1-million-element list) succeeds within the process budget.
  - [x] Exceeding the budget produces the documented failure, not a native
    crash.
- Tests
  - [x] Golden programs for deep body recursion and for the budget limit.
- Evidence (2026-10-05): `maint-29` unchanged at `21776803`. Step 19's frames
  already make body recursion independent of the native stack; this step adds
  the evidence. OTP golden `executables_deep_recursion`: 200,000-level body
  recursion (depth chosen on user request: beyond an 8 MiB native stack at 42 B
  per native frame) building a list and taking its length and sum, mutual
  recursion, a nested tuple measured recursively, and an exception unwinding
  200,000 frames to a handler. The budget run (`forever/1` never returns) is
  authored, as OTP cannot show it: exit 70 and
  `erlangaot: runtime failure: entry call failed: resource_limit`
  ([executables](../docs/executables.md#exit-status)) after about 1.1 million
  frames of 15 words. Golden runs may now be `"authored": true`
  (`regenerate.py` keeps them). A 1,000,000-element list build/len/sum ran
  once by hand (O0, Debug runtime, 6.5 s). Fresh Windows x64 Debug (clang-cl):
  fast CTest 154/154, full 158/158; Lizard 0 warnings; tidy 278 units pass
  (shared with step 19, no production code changed). Logs
  `build/plan11-step19/`.

<a id="step-21"></a>

### 21. Lower list comprehensions

Backlog: F13, F16. Depends on: [19](#step-19).

Include filters, multiple generators, pattern generators, strict generators
(`<:-`) and zip generators (`&&`) as accepted by OTP 29.

- Success criteria
  - [x] Results and evaluation order match OTP, including skipped non-matching
    elements and strict-generator errors.
  - [x] Long inputs run in bounded stack.
- Tests
  - [x] Golden programs for each generator kind, nested comprehensions and
    100k-element inputs.
- Evidence (2026-10-06): `maint-29` fetched, unchanged at `21776803`. Semantic
  views `semantic/comprehensions` (qualifiers, zip parts, generator inputs/
  patterns/strictness, templates); `expression_children` lists generator
  inputs, filters and templates, so calls, atoms, inference and specialization
  see them. Binding analysis saves and restores the scope around a
  comprehension and binds each qualifier's generator patterns as one fresh
  candidate (`BindingCandidate::fresh`: new identities shadow outer names);
  templates are siblings. Guard analysis classifies filters with OTP's
  `is_guard_test` rule (`Function::guard_filters`, legacy type tests at top
  level). Capability: list generators admitted; binary/map generators,
  zip-group filters (OTP `illegal_zip_generator` text) and match qualifiers
  (`compr_assign` text when the feature is off, capability when on) rejected.
  Lowering (`codegen/lowering_comprehensions`): one loop per generator
  qualifier with its cursor and the reversed accumulator in term slots (no PHIs,
  constant native and process stack), guard filters through `lower_guard`,
  others `true`/`false`/`{bad_filter, V}`, final `ContainerConstruction::reverse`
  (2). `ErrorReason` 16-18 `bad_generator`/`bad_filter`/`bad_generators`;
  strict rejection `{badmatch, E}` alone, `bad_generators` in a zip. OTP golden
  `executables_list_comprehensions` (6 runs: basic incl. shadowing, several
  templates, filter-only; patterns incl. strict and zip groups; guard vs body
  filters and evaluation order; 10 caught errors; 100,000-element inputs, a
  100,000-pair nested product and zip; uncaught `bad_generator`), full matrix.
  Semantic cases (shadowing, scope, zip filter, `compr_assign` both ways,
  guard use, binary generator). Former comprehension "unsupported" examples use
  record update `#r{}#r{a = 1}`; program `compile.txt` updated. Fresh Windows x64
  Debug (clang-cl): fast CTest 155/155; Lizard 0 warnings; tidy changed units
  pass. Logs `build/plan11-step21/`.

<a id="step-22"></a>

### 22. Lower binary and map comprehensions

Backlog: F13, F16. Depends on: [21](#step-21).

- Success criteria
  - [x] Binary generators/producers and map generators/producers match OTP,
    including partial bytes and duplicate map keys.
- Tests
  - [x] Golden programs for each combination of list, binary and map generators
    and producers.
- Evidence (2026-10-06): `maint-29` unchanged at `21776803`. Binary generators
  use match plans with `semantic::GeneratorPattern::element` (a final
  `binary`/`all` segment, `MatchPlan::rest`) and `skip` (OTP's skip pattern:
  segment values and repeated names ignored, floats read as integers); map
  generators keep the map, a position and its size in term slots and read
  `MapOperation::key_at`/`value_at`; a non-map input raises `bad_generator`
  before the loop. Each step tries the element, then a skip (relaxed generators
  advance, strict ones in a zip must match), then exhaustion (empty list, end of
  map, `<<>>` for strict and any bitstring for relaxed bit generators), then
  the error: `{badmatch, E}` (list head, bitstring rest, `{K, V}`),
  `bad_generator`, or `bad_generators` whose map entries are OTP's iterator
  chain (`MapOperation::iterator`). Producers accumulate like lists; a binary
  template must be a bitstring (`badarg`), the result is
  `BitOperation::concat`; maps evaluate the value before the key and finish
  with `MapOperation::from_list` (later keys win). `:=` map templates get OTP's
  error. OTP golden `executables_bit_map_comprehensions` (7 runs: all nine
  generator/producer combinations, partial bytes, sizes, UTF-8, floats, skips,
  map patterns, zips mixing kinds, 15 caught errors incl. iterator payloads,
  evaluation order, uncaught `badarg`), byte-identical to OTP; semantic cases
  (binary/map generators and producers, `:=` template). Fresh Windows x64 Debug
  (clang-cl): fast CTest 156/156, full 160/160 (275 s, `-j 12`); Lizard-all 0
  warnings; tidy-all found four findings in the new code, fixed and rechecked
  (changed units pass, affected tests 46/46 full mode). Phase E closed. Logs
  `build/plan11-step22/`.

## F. Memory management

<a id="step-23"></a>

### 23. Extend the root inventory to the execution model

Backlog: F02, F03, F04, F08–F11. Depends on: [17](#step-17), [8I](#step-8i).

Phase C delivered the layout walker (8C), process root words and explicit
host roots (8E) and stack roots (8F). Add the roots the step-17 model introduces: suspended frames or
continuations, in-flight error payloads and stack traces, and atom/module pins
held by heap cells.

- Success criteria
  - [ ] Every root owner of the step-17 model is enumerated by the collector;
    nothing outside the stack, process root words and listed owners holds heap
    words across a safe point.
- Tests
  - [ ] Runtime tests collecting while each root kind holds nested and shared
    graphs, followed by the 8C verifier.

<a id="step-24"></a>

### 24. Decide collection triggers and safepoints in generated code

Backlog: F04. Depends on: [23](#step-23). **Decision.**

The policy (single-heap copying with fragments) is fixed by 8A. Decide when
generated code collects: heap full at allocation, off-heap binary pressure,
`erlang:garbage_collect/0`, and which calls are safepoints versus critical
sections that keep using fragments.

- Success criteria
  - [ ] `docs/runtime-heap.md` defines triggers, safepoint placement, the
    reload rule for values held in registers, and failure behavior.
- Tests
  - [ ] IR prototype of one safepoint with reload on both word widths, recorded
    in the document.

<a id="step-25"></a>

### 25. Collect on explicit runtime request

Folded into [8H](#step-8h) on 2026-10-04; no separate commit.

<a id="step-26"></a>

### 26. Collect from generated code

Backlog: F02, F03, F04. Depends on: [24](#step-24), [20](#step-20), [8H](#step-8h).

Implement the step-24 safepoints: publish live values in stack frames before
collection, reload them afterwards and retry the failed allocation. Allocation
at a safepoint collects instead of creating a fragment; fragments remain for
critical sections and message delivery.

- Success criteria
  - [ ] Generated code reloads its frame base after safepoints, and the 8F
    segments are replaced by the step-17 stack form.
  - [ ] Allocation-heavy loops run with a bounded heap.
  - [ ] Values in recursive frames and error payloads survive repeated
    collections.
- Tests
  - [ ] Golden programs allocating far more than the heap budget while keeping
    a small live set.
  - [ ] Small-heap stress at O0/O2 with deep recursion and nested terms.

<a id="step-27"></a>

### 27. Report heap exhaustion as a defined failure

Backlog: F04. Depends on: [26](#step-26).

- Success criteria
  - [ ] A live set exceeding the process budget fails with the documented
    outcome after collection, and the runtime stays usable for teardown.
- Tests
  - [ ] Golden program growing a retained list past the budget; exit status
    and report checked.

<a id="step-28"></a>

### 28. Copy term graphs between heaps

Backlog: F05, F08–F11. Depends on: [8H](#step-8h).

Extend `Term::copy_to` to compound terms: size the source graph with the 8C
walker, then copy into a destination heap fragment (BEAM `size_object` and
`copy_struct`) with preserved internal sharing, destination budgets and
rollback. Off-heap binaries gain a reference instead of being copied.

- Success criteria
  - [ ] Copies compare equal and survive destruction or collection of the source
    process.
  - [ ] A failed copy leaves both heaps and resource counts unchanged.
- Tests
  - [ ] Runtime tests copying nested/shared graphs, maps, large binaries and
    partial-byte bitstrings between contexts.
  - [ ] Destination exhaustion injected mid-copy.

## G. Records, function values, dynamic calls

<a id="step-29"></a>

### 29. Lower record updates

Backlog: F17. Depends on: [9](#step-9).

- Success criteria
  - [ ] `Expr#r{f = V}` checks the record shape and raises `{badrecord, Value}`
    like OTP; evaluation order matches OTP.
- Tests
  - [ ] Golden programs for single/multi-field updates, nested records and
    wrong-record values.

<a id="step-30"></a>

### 30. Implement `record_info/2`

Backlog: F17. Depends on: [29](#step-29).

- Success criteria
  - [ ] `record_info(fields | size, r)` resolves at compile time; invalid uses
    are diagnosed like OTP.
- Tests
  - [ ] Golden programs and CLI diagnostics for unknown records and non-literal
    arguments.
 
<a id="step-31"></a>

### 31. Implement native, qualified and inferred record forms

Backlog: F03, F12, F17. Depends on: [30](#step-30), [23](#step-23).

Scope the OTP 29 record forms beyond ordinary tuple records first; split into
sub-steps if more than one representation is needed.

- Success criteria
  - [ ] Each selected form has construction, access, update, matching,
    comparison, printing, tracing and copying rules.
  - [ ] Unselected forms keep explicit unavailable diagnostics.
- Tests
  - [ ] Golden programs per selected form, including errors and cross-module
    use.

<a id="step-32"></a>

### 32. Implement function values without captures

Backlog: F03, F12, F18. Depends on: [23](#step-23), [19](#step-19).

Represent `fun F/A` and `fun M:F/A` with retained code ownership.

- Success criteria
  - [ ] Values compare, print and pass `is_function/1,2` like OTP.
  - [ ] Calling with wrong arity raises `{badarity, …}`; a non-function raises
    `{badfun, …}`.
- Tests
  - [ ] Golden programs passing, storing and calling local and remote function
    values.

<a id="step-33"></a>

### 33. Implement closures with captured variables

Backlog: F03, F18. Depends on: [32](#step-32), [26](#step-26).

- Success criteria
  - [ ] Anonymous funs with clauses and guards capture values that survive the
    creator's return and collection.
- Tests
  - [ ] Golden programs for captures, multi-clause funs, higher-order helpers
    and closures created in loops.
  - [ ] Small-heap stress keeping closures alive across collections.

<a id="step-34"></a>

### 34. Implement named funs

Backlog: F18, F21. Depends on: [33](#step-33).

- Success criteria
  - [ ] `fun Name(…) -> … Name(…) end` recurses, including tail recursion in
    constant stack.
- Tests
  - [ ] Golden programs for recursive and tail-recursive named funs.

<a id="step-35"></a>

### 35. Implement dynamic calls

Backlog: F19. Depends on: [32](#step-32).

Support `Fun(Args)`, `Mod:Fun(Args)` with runtime operands, and
`erlang:apply/2,3`.

- Success criteria
  - [ ] Lookup pins the module for the call; missing targets raise `undef`;
    non-atom operands raise `badarg`/`badfun` as OTP.
- Tests
  - [ ] Golden programs for each call form and failure.

## H. Builtins and libraries

<a id="step-36"></a>

### 36. Implement the generic production builtin bridge

Backlog: F26. Depends on: [11](#step-11).

Register production builtins by module/name/arity and call them from generated
code with checked status and owned results.

- Success criteria
  - [ ] Generated code calls a registered builtin; unregistered names keep the
    unavailable diagnostic; failures use the checked error channel.
- Tests
  - [ ] Golden programs calling bridge builtins with valid and invalid
    arguments.
  - [ ] Focused test for duplicate registration and failure rollback.

<a id="step-37"></a>

### 37. Add the term-access builtin family

Backlog: F26. Depends on: [36](#step-36).

`element/2`, `setelement/3`, `tuple_size/1`, `make_tuple/2,3`,
`tuple_to_list/1`, `list_to_tuple/1`, `hd/1`, `tl/1`, `length/1`, `map_get/2`,
`map_size/1` and `is_map_key/2` in body context, plus the list operators
`++`/`--` (`erlang:'++'/2`, `erlang:'--'/2`).

- Success criteria
  - [ ] Results and `badarg` errors match OTP.
- Tests
  - [ ] Golden call/result corpus regenerated from OTP with boundary and invalid
    arguments.

<a id="step-38"></a>

### 38. Add the conversion builtin family

Backlog: F11, F26. Depends on: [36](#step-36).

Atom, integer, float, list, binary and string conversions
(`atom_to_list/1`, `list_to_atom/1`, `integer_to_list/1,2`,
`list_to_integer/1,2`, `float_to_list/1,2`, `binary_to_list/1`,
`list_to_binary/1`, `iolist_to_binary/1`, `term_to_binary/1` excluded unless
selected).

- Success criteria
  - [ ] Results and errors match OTP; atom-table limits fail as documented.
- Tests
  - [ ] Golden call/result corpus regenerated from OTP.

<a id="step-39"></a>

### 39. Ship a project-owned library subset for `lists` and `maps`

Backlog: F08, F26. Depends on: [21](#step-21), [35](#step-35).

Write original Erlang implementations (not OTP copies) of commonly used
functions, compiled and linked with user programs. Start with `lists:reverse,
map, foldl, foldr, filter, member, keyfind, sort, seq, nth, append` and
`maps:get, put, find, keys, values, fold, from_list, to_list`.

- Success criteria
  - [ ] Library modules build with the compiler and link automatically into
    executables; results match OTP.
- Tests
  - [ ] Golden call/result corpus regenerated from OTP for every function.

<a id="step-40"></a>

### 40. Add console output through `io`

Backlog: F26. Depends on: [4](#step-4), [36](#step-36).

`io:put_chars/1`, `io:format/1,2` with `~w ~p ~s ~n ~b ~B ~c ~~`.

- Success criteria
  - [ ] Output matches OTP for the supported directives; unsupported directives
    and bad arguments raise OTP-like errors.
- Tests
  - [ ] Golden programs printing each directive, nested terms with `~p` line
    breaking, and Unicode strings.

<a id="step-41"></a>

### 41. Add typed native callables for builtin implementations

Backlog: F27. Depends on: [37](#step-37), [38](#step-38).

Use the builtin families as the concrete use case: typed C++ signatures with
checked argument conversion and generic Term fallback.

- Success criteria
  - [ ] Existing builtins migrate to typed wrappers with identical behavior;
    wrong types become `badarg`; C++ exceptions never cross the generated ABI.
- Tests
  - [ ] Existing builtin goldens pass unchanged.
  - [ ] Focused tests for conversion failure, expired handles and a throwing
    callback.

## I. Processes and messaging

<a id="step-42"></a>

### 42. Implement pid and reference identities

Backlog: F03, F07, F12. Depends on: [23](#step-23), [28](#step-28).

`self/0` (main process only) and `make_ref/0`, with comparison, printing,
tracing and copying.

- Success criteria
  - [ ] Identities are unique, compare and print like OTP; forged or stale
    words are rejected.
- Tests
  - [ ] Golden programs comparing, sorting and storing pids/references in maps.
  - [ ] Runtime tests for forged, stale and foreign identities.

<a id="step-43"></a>

### 43. Run spawned processes on a cooperative executor

Backlog: F01, F22. Depends on: [42](#step-42), [33](#step-33).

Implement `spawn/1,3` with per-process heaps, reduction counting and yields per
the step-17 model; startup runs the entry as the first process and exits when
it finishes.

- Success criteria
  - [ ] Many processes interleave on one thread; each heap is isolated.
  - [ ] A crashing process does not affect others.
- Tests
  - [ ] Golden programs spawning 10k processes and long-running busy loops
    that must interleave.
  - [ ] Teardown with live processes releases every heap.

<a id="step-44"></a>

### 44. Define process exit and crash reports

Backlog: F22. Depends on: [43](#step-43).

- Success criteria
  - [ ] Normal return, `exit/1` and uncaught errors terminate the process with
    OTP-like reasons; non-normal termination writes an error report.
- Tests
  - [ ] Golden programs for each termination kind with stderr checked.

<a id="step-45"></a>

### 45. Implement the signal inbox and message send

Backlog: F05, F24. Depends on: [43](#step-43), [28](#step-28).

`Pid ! Msg` and `erlang:send/2`; every message, including self-send, enters the
signal inbox and is copied by the step-28 service into a heap fragment of the
receiver, merged into its heap at the next collection.

- Success criteria
  - [ ] Per-sender order is preserved; sending to a dead process succeeds
    silently; invalid destinations raise `badarg`.
- Tests
  - [ ] Golden programs for ping-pong, fan-in ordering and self-send.
  - [ ] Copy failure in the receiver is handled per contract.

<a id="step-46"></a>

### 46. Implement selective receive without timeout

Backlog: F13, F25. Depends on: [45](#step-45).

- Success criteria
  - [ ] The first matching message (patterns and guards) is removed; unmatched
    messages stay in order; a process with no match suspends and wakes on
    arrival.
- Tests
  - [ ] Golden programs for selective receive out of order, repeated scans and
    many unmatched messages.

<a id="step-47"></a>

### 47. Implement `receive … after` timeouts

Backlog: F25. Depends on: [46](#step-46).

- Success criteria
  - [ ] `after 0`, finite and `infinity` timeouts behave as OTP; a message
    arriving before expiry is taken, never lost.
- Tests
  - [ ] Golden programs for each timeout kind and `timer`-style sleeps built on
    `receive after`; no exact-time assertions.

<a id="step-48"></a>

### 48. Implement links, exit signals and `trap_exit`

Backlog: F07, F22. Depends on: [44](#step-44), [46](#step-46).

`link/1`, `unlink/1`, `spawn_link/1,3`, `exit/2`,
`process_flag(trap_exit, Bool)`.

- Success criteria
  - [ ] Exit propagation, `kill`, `normal` and trapped `{'EXIT', Pid, Reason}`
    messages match OTP.
- Tests
  - [ ] Golden programs for linked crash chains, trapping supervisors and
    `exit/2` variants.

<a id="step-49"></a>

### 49. Implement monitors

Backlog: F22. Depends on: [48](#step-48).

`monitor/2` (process), `demonitor/1,2`, `spawn_monitor/1,3`.

- Success criteria
  - [ ] `'DOWN'` messages carry OTP reasons, including `noproc`; `flush`
    removes a pending `'DOWN'`.
- Tests
  - [ ] Golden programs for monitored exits, already-dead targets and
    demonitor races.

<a id="step-50"></a>

### 50. Implement registered process names

Backlog: F26. Depends on: [45](#step-45).

`register/2`, `unregister/1`, `whereis/1`, `registered/0`, send to a name.

- Success criteria
  - [ ] Name conflicts and sends to unknown names raise `badarg` as OTP; names
    are released when the process exits.
- Tests
  - [ ] Golden programs for registration, re-registration after exit and
    error cases.

<a id="step-51"></a>

### 51. Collect garbage with mailboxes and suspended processes

Backlog: F02, F04. Depends on: [26](#step-26), [47](#step-47).

Trace message queues, in-transit messages, receive cursors and suspended
continuations as roots.

- Success criteria
  - [ ] Suspended and message-heavy processes survive collection with all
    messages and cursor state intact.
- Tests
  - [ ] Small-heap stress: processes holding large mailboxes and suspended in
    receive while collections run.

<a id="step-52"></a>

### 52. Enable identity-dependent guards

Backlog: F14. Depends on: [42](#step-42), [31](#step-31), [32](#step-32).

`self/0` in guards, `node/0,1` (returning `nonode@nohost`), native
`is_record/1`, and positive `is_pid/1`, `is_reference/1`, `is_function/1,2`.

- Success criteria
  - [ ] The four gated catalog signatures become available; results match OTP.
- Tests
  - [ ] Guard golden corpus extended with the new rows.

<a id="step-53"></a>

### 53. Decide port identity scope

Backlog: F07. Depends on: [42](#step-42). **Decision.**

- Success criteria
  - [ ] Document whether ports exist (likely: no external ports; `is_port/1`
    is always false; `open_port/2` reports unavailable).
- Tests
  - [ ] CLI/golden checks for the documented boundary.

## J. Multi-worker scheduling

<a id="step-54"></a>

### 54. Synchronize the atom table

Backlog: F06. Depends on: [43](#step-43).

- Success criteria
  - [ ] Concurrent intern and lookup are safe and keep stable identities.
- Tests
  - [ ] Multi-threaded runtime stress creating overlapping atoms.

<a id="step-55"></a>

### 55. Synchronize code-server publication and lookup

Backlog: F28. Depends on: [43](#step-43).

- Success criteria
  - [ ] Lookups and calls run concurrently with publication; module pins last
    through invocation and closure lifetime.
- Tests
  - [ ] Multi-threaded stress for duplicate registration, lookup and teardown.

<a id="step-56"></a>

### 56. Run processes on multiple scheduler workers

Backlog: F23. Depends on: [54](#step-54), [55](#step-55), [51](#step-51).

Per-worker run queues with work stealing (or the documented alternative) and a
worker count option.

- Success criteria
  - [ ] Programs produce the same results with 1 and N workers; CPU-bound
    processes use multiple cores.
- Tests
  - [ ] Existing process goldens run with several worker counts.
  - [ ] Fairness check: a busy loop cannot starve a receiver.

<a id="step-57"></a>

### 57. Handle cross-worker wakeups, timers and shutdown

Backlog: F23, F25. Depends on: [56](#step-56).

- Success criteria
  - [ ] No lost wakeups for messages, exit signals or timeouts across workers;
    shutdown stops workers and releases every process.
- Tests
  - [ ] Repeated stress for arrival-versus-timeout races and concurrent
    teardown (also used under ThreadSanitizer in step 68).

## K. End-to-end projects

<a id="step-58"></a>

### 58. Run the target fixture projects end to end

Backlog: F01, V03. Depends on: [2](#step-2), [57](#step-57).

- Success criteria
  - [ ] Every step-2 fixture builds through its project manifest and matches
    its OTP golden at O0/O2 with 1 and N workers.
  - [ ] Remaining gaps found by fixtures are added to the backlog with owners.
- Tests
  - [ ] Fixtures run through the step-8 runner in normal CTest.

## L. Optimization and tooling

<a id="step-59"></a>

### 59. Make specialization remove real source checks

Backlog: F29. Depends on: [58](#step-58).

- Success criteria
  - [ ] Proven profiles remove tag/shape checks in new operations (arithmetic,
    tuple access, list loops) with generic fallback and existing limits.
- Tests
  - [ ] Same goldens pass with specialization on/off; code size and compile
    time recorded descriptively, not gated.

<a id="step-60"></a>

### 60. Emit source-level debug information

Backlog: F30. Depends on: [58](#step-58).

- Success criteria
  - [ ] Executables carry line tables through macros/includes; a debugger
    breaks on an Erlang line and shows the Erlang call stack.
- Tests
  - [ ] Scripted debugger session (LLDB/GDB where available) on a golden
    program; line-table inspection test runs everywhere.

<a id="step-61"></a>

### 61. Add opt-in profiling

Backlog: F31. Depends on: [58](#step-58).

- Success criteria
  - [ ] An opt-in mode attributes time/reductions per function and per
    process; disabled mode leaves output and artifacts unchanged.
- Tests
  - [ ] Profile a known hot function in a golden program and check it ranks
    first; byte-identical artifacts when disabled.

<a id="step-62"></a>

### 62. Integrate link-time optimization

Backlog: F32. Depends on: [7](#step-7), [58](#step-58).

- Success criteria
  - [ ] An `--lto` option links bitcode with descriptors, exports and startup
    intact on supported toolchains; unsupported targets report it.
- Tests
  - [ ] Fixture goldens pass with LTO; size/build time recorded.

## M. Validation closure

<a id="step-63"></a>

### 63. Validate on Linux x86-64

Backlog: V01. Depends on: [58](#step-58).

- Success criteria
  - [ ] Fresh build, full gate and executable goldens pass; versions and counts
    published in `docs/validation.md`.
- Tests
  - [ ] Full gate plus fixture projects at O0/O2.

<a id="step-64"></a>

### 64. Validate 32-bit x86 (Windows x86 and Linux x86)

Backlog: V01. Depends on: [63](#step-63).

- Success criteria
  - [ ] Same as step 63 with 32-bit word width; 28-bit small-integer boundaries
    exercised natively.
- Tests
  - [ ] Full gate plus integer-boundary and fixture goldens.

<a id="step-65"></a>

### 65. Validate Linux AArch64 and 32-bit ARM

Backlog: V01. Depends on: [63](#step-63).

- Success criteria
  - [ ] Same as step 63 on each architecture, or the missing runner recorded
    as a gap.
- Tests
  - [ ] Full gate plus fixture goldens per architecture.

<a id="step-66"></a>

### 66. Validate macOS Apple Silicon

Backlog: V01. Depends on: [63](#step-63).

- Success criteria
  - [ ] Same as step 63 on macOS arm64.
- Tests
  - [ ] Full gate plus fixture goldens.

<a id="step-67"></a>

### 67. Run compiler and frontend sanitizers

Backlog: V02. Depends on: [63](#step-63).

Use a host/SDK combination without the recorded MSVC annotation and allocator
conflicts (Linux is the likely choice).

- Success criteria
  - [ ] Full compiler+runtime ASan, UBSan and LeakSanitizer runs pass; findings
    are fixed, not suppressed; instrumentation scope is documented.
- Tests
  - [ ] Full CTest under each sanitizer configuration.

<a id="step-68"></a>

### 68. Run ThreadSanitizer on the multi-worker runtime

Backlog: V02. Depends on: [57](#step-57), [67](#step-67).

- Success criteria
  - [ ] Process, scheduler and code-server stress tests report no races.
- Tests
  - [ ] Step-56/57 stress tests under TSan, repeated.

<a id="step-69"></a>

### 69. Broaden OTP compatibility evidence

Backlog: V03. Depends on: [58](#step-58).

- Success criteria
  - [ ] Selected applicable upstream Common Test suites run against a matching
    built OTP, kept separate from ErlangAoT differential comparisons.
  - [ ] Differential goldens cover every feature enabled by this plan;
    exclusions are published.
- Tests
  - [ ] Opt-in upstream run with recorded commands; normal CTest stays OTP-free.

<a id="step-70"></a>

### 70. Close the test migration ledger

Backlog: V04. Depends on: [58](#step-58).

- Success criteria
  - [ ] Every remaining adapter or synthetic success test either has equivalent
    executable coverage and is removed, or has a recorded reason to stay.
- Tests
  - [ ] Full gate after removals; ledger dispositions cite the replacing tests.

## N. Optional scope decisions

Each step records a decision in its contract document. If an item is selected,
write its own small implementation plan before coding.

<a id="step-71"></a>

### 71. Decide dynamic modules and code upgrades

Backlog: D01. Depends on: [55](#step-55). **Decision.**

- Success criteria
  - [ ] Static-only, native dynamic libraries, or an upgrade model chosen;
    static-only failures for `code:load_*` documented if omitted.
- Tests
  - [ ] Golden check of the documented boundary behavior.

<a id="step-72"></a>

### 72. Decide atom collection

Backlog: D02. Depends on: [54](#step-54). **Decision.**

- Success criteria
  - [ ] Bounded permanent atoms kept, or collection roots and policy defined.
- Tests
  - [ ] Atom-limit golden for the chosen behavior.

<a id="step-73"></a>

### 73. Decide behavior-changing attributes and transforms

Backlog: D03. Depends on: [58](#step-58). **Decision.**

- Success criteria
  - [ ] Per-attribute decision (`compile` options, `parse_transform`,
    `on_load`, others); rejected ones keep diagnostics.
- Tests
  - [ ] CLI diagnostics for each rejected attribute.

<a id="step-74"></a>

### 74. Decide public stage interchange

Backlog: D04. Depends on: [58](#step-58). **Decision.**

- Success criteria
  - [ ] A concrete consumer named, or deferral recorded; inspection dumps stay
    non-stable.
- Tests
  - [ ] None beyond the gate unless selected.

<a id="step-75"></a>

### 75. Confirm intermediate-stage reader reservations

Backlog: D05. Depends on: [74](#step-74). **Decision.**

- Success criteria
  - [ ] Directory reservations remain the only artifact per AGENTS.md, unless
    a D04 consumer requires a reader.
- Tests
  - [ ] None beyond the gate.

<a id="step-76"></a>

### 76. Decide C/FFI interoperability

Backlog: D06. Depends on: [58](#step-58). **Decision.**

- Success criteria
  - [ ] An external caller and minimal API named, or deferral recorded.
- Tests
  - [ ] None beyond the gate unless selected.

<a id="step-77"></a>

### 77. Decide project schema extensions

Backlog: D07. Depends on: [58](#step-58). **Decision.**

- Success criteria
  - [ ] Demand recorded for each extension (profiles, dependencies, exclusions,
    packages, watch/cache, parallel builds); selected items get their own plan.
- Tests
  - [ ] None beyond the gate unless selected.

## O. Final closure

<a id="step-78a"></a>

### 78A. Reset every versioned ABI name to v1

Backlog: all. Depends on: steps 1–70 and any selected optional work. Inserted
2026-10-05.

The project has never been released, so no earlier generated-code contract has
to stay loadable. Collapse every version marker to v1: runtime service symbols
(`erlang_aot_roots_enter_v5`, `erlang_aot_roots_leave_v4`,
`erlang_aot_raise_v2`, `erlang_aot_call_failed_v2`,
`erlang_aot_register_module_v4`, `erlang_aot_atom_v3`,
`erlang_aot_exception_v2`, `erlang_aot_reraise_v2` and the rest), C++
namespaces such as `abi::v1`/`v2`, `abi::v1::version` and descriptor revision
numbers, the generated `eav1_` prefixes if any other revision exists, and the
revision tables in the docs.

- Success criteria
  - [ ] No `_v2`-or-later suffix, versioned namespace or revision number above
    1 remains in sources, generated IR, tests, fixtures or docs; the ABI
    document describes one revision 1 without a revision history.
  - [ ] `runtime_symbols.hpp` aliases mirror the renamed `abi/include`
    declarations; no mangled spelling is hardcoded elsewhere.
- Tests
  - [ ] `tests/compiler/codegen/mangling.cpp` spellings, cross-target import
    checks, native consumers and every test calling a service directly use the
    v1 names, checked against Clang for every target ABI and width.
  - [ ] Fresh full gate and `check-quality-all` pass.

<a id="step-78"></a>

### 78. Publish the final implementation and validation boundary

Backlog: all. Depends on: steps 1–70, [78A](#step-78a) and any selected optional work.

- Success criteria
  - [ ] `00-finished.md`, `01-todo.md`, `arch.md`, `files.md`, contracts and
    examples reflect exactly what is implemented, validated, deferred or
    omitted.
  - [ ] README shows building and running a multi-process Erlang program as an
    executable.
- Tests
  - [ ] Fresh full gate on every available host; README commands executed as
    written.
