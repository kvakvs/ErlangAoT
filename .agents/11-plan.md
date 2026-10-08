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
  header dependents). When a phase or major feature completes, run full-mode
  CTest (`ctest --preset debug -j <N>`; each test takes two slots, so N/2 run
  at once) instead of fast mode, never both (full covers every fast run), and
  `check-quality-all`. Lizard and clang-tidy pass
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
| I. Processes and messaging | [42](#step-42)–[53](#step-53), [43A](#step-43a) | F02, F04, F05, F07, F14, F22, F24–F26 |
| J. Multi-worker scheduling | [54](#step-54)–[57](#step-57) | F06, F23, F25, F28 |
| K. End-to-end projects | [58](#step-58) | F01, V03 |
| L. Optimization and tooling | [59](#step-59)–[62](#step-62), [62A](#step-62a) | F29–F33 |
| M. Validation closure | [63](#step-63)–[70](#step-70) | V01–V04 |
| N. Optional scope decisions | [71](#step-71)–[77](#step-77) | D01–D07 |
| O. Final closure | [78A](#step-78a), [78](#step-78) | all |

---

## Completed steps 1–35 (compact record)

Full step texts, criteria and per-step evidence are in Git history (last full
versions: steps 1–8G at `a4e07bb`, steps 8H–27E at `9decf7a`, steps 28–35 at
`c82066f`). Every step below passed the common gate; per-step logs are in
`build/plan11-step*/`.

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

Inserted 2026-10-04 to replace the phase-B heap (chunk list, per-cell index,
fixed bitstring cells, pinning host terms) with a BEAM-style design
(`docs/runtime-heap.md`): parseable flat word areas, one heap block plus
fragments per process, binaries over 64 bytes in shared off-heap buffers.
Generated-code ABI and goldens stayed unchanged.

<a id="step-8a"></a>

### 8A. Decide the classic process heap contract

Done 2026-10-04. `docs/runtime-heap.md`: header word (5 kind bits, count from
bit 7), per-kind cell table, filler, areas, ERTS sizing from 233 words, one
`limit_bytes` budget including off-heap buffers, admission, safe points.
Full-only CTest `runtime_heap_measurements`.

<a id="step-8b"></a>

### 8B. Split binary cells and add the off-heap list

Done 2026-10-04. `HeapBinaryCell` (up to 64 bytes inline) and 6-word
`RefcBinaryCell` (`shared_ptr<const BinaryBuffer>`, `next_`) on the
`memory/off_heap` list; buffers charged via `off_heap_words`. CTest
`runtime_off_heap`.

<a id="step-8c"></a>

### 8C. Make heap areas parseable and add a heap walker

Done 2026-10-04. `memory/heap_walk` (`parse_cell`, `walk`) and
`ProcessHeap::verify()` (`HeapCensus` or `corrupt_heap`); `BoxedKind::filler`.
CTest `runtime_heap_walk`.

<a id="step-8d"></a>

### 8D. Admit heap words by header instead of the object index

Done 2026-10-04. Admission = word-aligned address below an area top plus a
header/cons shape matching the tag; no start bitmap. CTest
`runtime_admission`.

<a id="step-8e"></a>

### 8E. Hold host terms as raw words between safe points

Done 2026-10-04. `Term` = word + borrowed `HeapStorage *` + weak lifetime +
collection count (`expired_context`, `stale_term`); no storage pin.
`ProcessContext::visit_roots` enumerates every root.

<a id="step-8f"></a>

### 8F. Move generated root frames onto a process stack

Done 2026-10-04. Interim segmented root stack; replaced by the flat
`ProcessStack` in step 19.

<a id="step-8g"></a>

### 8G. Replace chunks with a contiguous heap and heap fragments

Done 2026-10-04. `HeapStorage`: one heap block (first reservation,
`max(min_heap_words, request)`) plus a fragment chain; `HeapOptions
{min_heap_words = 233, limit_bytes}`; rollback drops new areas. CTest
`runtime_heap_fragments`.

<a id="step-8h"></a>

### 8H. Collect on explicit host request with a copying collector

Done 2026-10-04 (absorbs former step 25). Cheney copy (`memory/heap_collect`:
`Copier`, `heap_size_at_least`) into one new block at a safe point, roots
rewritten, off-heap list swept, fragments freed; `unsafe_point` and new-block
OOM change nothing. CTests `runtime_collection`, `runtime_lifecycle_failure`.

<a id="step-8i"></a>

### 8I. Close the heap rework

Done 2026-10-04. Docs, `arch.md`, `files.md` describe the new heap; kernel
re-measured (700,000 live words collected in about 56 ms). Phase C close: full
143/144 (one load timeout, passed alone), quality-all 276 units.

### D. Control flow and exceptions

<a id="step-9"></a>

### 9. Lower `begin`/`end` blocks and `case` expressions

Done 2026-10-04. Case clauses reuse the one-input body plan; one binding
identity per name across clauses, exports in `Function::exports`, joined with
`SSAUpdater`; `ErrorReason::case_clause` (9). OTP goldens
`executables_case_select`, `executables_case_scope`; bindings corpus +8 rows.

<a id="step-10"></a>

### 10. Lower `if` expressions

Done 2026-10-05. `semantic::branch_clauses` presents case and if clauses
uniformly; `ErrorReason::if_clause` (10). OTP golden `executables_if_select`;
bindings corpus +4 rows.

<a id="step-11"></a>

### 11. Raise exceptions from source

Done 2026-10-05. `error/1,2,3`, `exit/1`, `throw/1` as body builtins
(`semantic::body_builtin`, auto-import rules) through `erlang_aot_raise_v2`
with `ErrorReason` 11-13 (whole-reason payload); startup prints
`uncaught exception <class>: <reason>`. OTP golden `executables_raise_classes`.

<a id="step-12"></a>

### 12. Lower `catch Expr`

Done 2026-10-05. `ExpressionLowering::handler` while the protected expression
lowers (fresh badarg/badarith exits); `erlang_aot_catch_v1` builds the value
and clears the channel, halts and runtime failures continue outward; inner
bindings unsafe. OTP golden `executables_catch_values`; bindings corpus +4
rows.

<a id="step-13"></a>

### 13. Lower `try … of … catch`

Done 2026-10-05. Only the body is protected; the handler takes class and
reason (`erlang_aot_exception_v*`), catch clauses match from pre-try bindings,
no match re-raises (`erlang_aot_reraise_v*`), `of` exhaustion raises
`try_clause` (14). OTP golden `executables_try_catch`; bindings corpus +6
rows.

<a id="step-14"></a>

### 14. Lower `try … after`

Done 2026-10-05. A second protected scope (`afters`) around body and clauses;
the after body is lowered on the normal path and again from the after handler;
its exceptions replace the original. OTP golden `executables_try_after`;
native `codegen_after_fault_O0/O2` (budget failure inside `after`).

<a id="step-15"></a>

### 15. Provide stack traces and `erlang:raise/3`

Done 2026-10-05. Frames name their function (`FrameDescriptor`); the innermost
8 named frames are captured at `GeneratedCallState::fail`, built as
`[{M, F, Arity, []}]` on demand; `Class:Reason:Stack`, `erlang:raise/3` (BEAM
validation) and `error/2,3` arguments. Differences in `docs/abi.md#stack-traces`.
OTP golden `executables_stack_traces`.

<a id="step-16"></a>

### 16. Lower `maybe` expressions

Done 2026-10-05. `semantic::maybe_operands` plus `branch_clauses` for `else`;
failed `?=` goes to `maybe.else`; no `else` match raises `else_clause` (15);
feature gating is the preprocessor keyword switch. OTP golden
`executables_maybe_else`. Phase D close: full 155/155, quality-all 277 units.

### E. Execution model, recursion, comprehensions

<a id="step-17"></a>

### 17. Decide the frame and continuation model

Done 2026-10-05 (decision, `docs/execution-model.md`). Explicit frames on one
flat moving per-process stack (4-word header: previous offset, descriptor,
resume, handler), arguments in X registers, entry plus resume-switch body per
function, only `musttail` transfers; trampoline fallback. Prototype
`tests/prototypes/execution_model/` on 7 targets at O0/O2; native calls and
coroutines rejected.

<a id="step-18"></a>

### 18. Accept recursive call graphs in analysis

Done 2026-10-05. Calls ordered by strongly connected components
(`CallGraph::components`, iterative Tarjan); recursive components infer from
`none()` for at most 16 rounds, then widen to `term()`. OTP golden
`executables_recursion`; `codegen_types` recursion checks.

<a id="step-19"></a>

### 19. Implement proper tail calls

Done 2026-10-05. Post-pass `codegen/frames` (`lower_frames`) turns native form
into `<sym>.body` code with resume switches, spills and `musttail` transfers
via `erlang_aot_enter/tail/return_v1`; descriptors `<sym>.frame`; runtime
`ProcessStack` (flat vector, 256 registers, bottom frame per invocation); ABI
version 5. OTP golden `executables_tail_calls` (2,000,000 iterations, reduced
on user request); `codegen_cross_targets` checks `musttail` on 7 targets.

<a id="step-20"></a>

### 20. Support deep non-tail recursion

Done 2026-10-05. Evidence only: step-19 frames make depth independent of the
native stack. OTP golden `executables_deep_recursion` (200,000 levels); its
authored stack-budget run was removed by the step-27 correction.

<a id="step-21"></a>

### 21. Lower list comprehensions

Done 2026-10-06. `semantic/comprehensions` views; fresh generator bindings;
guard filters via OTP's `is_guard_test`; loops in `codegen/lowering_comprehensions`
with cursor and reversed accumulator in term slots; `ErrorReason` 16-18
(`bad_generator`, `bad_filter`, `bad_generators`). OTP golden
`executables_list_comprehensions` (100,000-element inputs).

<a id="step-22"></a>

### 22. Lower binary and map comprehensions

Done 2026-10-06. Binary generators via match plans with element/skip patterns;
map generators by position (`MapOperation::key_at/value_at`, OTP iterator
payloads); producers finish with `BitOperation::concat` or
`MapOperation::from_list`. OTP golden `executables_bit_map_comprehensions`.
Phase E close: full 160/160.

### F. Memory management

<a id="step-23"></a>

### 23. Extend the root inventory to the execution model

Done 2026-10-06. Roots: frame term slots, live registers
(`ProcessStack::keep_registers`), failure payload/arguments/stack, explicit
roots (`docs/runtime-heap.md#roots-and-safe-points`); `SafePoint` scopes allow
collection inside generated code. `runtime_collection` `root_owners`.

<a id="step-24"></a>

### 24. Decide collection triggers and safepoints in generated code

Done 2026-10-06 (decision, `docs/runtime-heap.md#collection-in-generated-code`).
Safepoints only at function entry and comprehension loop heads
(`erlang_aot_safepoint_v1`); every service is a critical section using
fragments. Triggers: a fragment exists, or off-heap words reach the virtual
binary heap. Prototype `tests/prototypes/safepoint/`.

<a id="step-25"></a>

### 25. Collect on explicit runtime request

Folded into [8H](#step-8h) on 2026-10-04; no separate commit.

<a id="step-26"></a>

### 26. Collect from generated code

Done 2026-10-06. `ProcessStack::safepoint` collects in `enter` (arguments as
register roots) and at loop heads; new blocks count stack words as live;
`lower_frames` spills crossing term values into term slots. OTP golden
`executables_garbage_collection` (each run allocates over 64 MiB with a small
live set); host harnesses keeping `Term`s use a large minimum heap.

<a id="step-27"></a>

### 27. Report heap exhaustion as a defined failure

Done 2026-10-06, corrected by the user the same day. No memory cap by
default: heap and stack grow until the host refuses (`out_of_memory`, exit
70); opt-in per-process budgets (`HeapOptions::limit_bytes`,
`StackOptions::limit_words`) fail with `resource_limit` only after
collection (`block_limit` keeps half of the free budget for fragments, capped
`binary_limit_words_`). No binary size or process-count caps; atoms 2^20 by
default, `--max-atoms` up to 2^26; runtime options from leading arguments and
`ERLANG_AOT_FLAGS` (`startup/options`, `--`, reserved `--args-file`). OTP
goldens `executables_heap_growth`, `executables_runtime_options`,
`executables_large_binaries`; golden runs may set `env`.

<a id="step-27a"></a>

### 27A. Runtime-wide memory limit and program-facing caps

Done 2026-10-06. Optional `RuntimeOptions::memory_limit_bytes`: one
`detail::RuntimeMemory` account charged by heap blocks, fragments, off-heap
buffers and stack capacity (`Runtime::memory_bytes()`); each process sees its
own storage plus what the limit leaves, so only the requesting process fails.
Options `--max-heap`, `--max-stack`, `--max-memory` set the process defaults
used by `create_context()`. Capped authored runs in `garbage_collection`,
`tail_calls`, `deep_recursion`; `runtime_collection` `shared_limit`. Gap:
`deep` has no capped success run (about 100 MiB at O0).

<a id="step-27b"></a>

### 27B. Remove list length caps

Done 2026-10-06. No list length cap and no comparison work cap; identical
words compare equal without a walk. `runtime_containers` `long_lists`
(1,000,001 elements) and `unbounded_comparison` (2^20 leaf pairs).

<a id="step-27c"></a>

### 27C. Match OTP's tuple arity limit

Done 2026-10-06. Public `MAX_TUPLE_ARITY` (16,777,215) beside `TermFactory`;
the construction service builds tuples from words (`TermFactory::tuple_words`).
`runtime_containers` `tuple_arity` at the boundary.

<a id="step-27d"></a>

### 27D. Remove map size and key-work caps

Done 2026-10-06. No map size or key-comparison budget; construction
stable-sorts by exact key order keeping the last value (skipped for ascending
keys); a header-capacity check bounds 32-bit maps; byte-aligned bitstrings
compare by bytes. `runtime_maps` `large_maps`.

<a id="step-27e"></a>

### 27E. Match OTP's big integer limit

Done 2026-10-06. ERTS limit (`BIG_ARITY_MAX` words: 4,194,240 bits on 64-bit,
4,194,272 on 32-bit); a larger result raises `error:system_limit` in bodies
and rejects guards (`ValueOutcome::system_limit` 3, `ErrorReason::system_limit`
19, codegen `checked_arithmetic`); an over-limit extracted segment does not
match. Compiler: one `INTEGER_BIT_LIMIT`; the lexer rejects longer literals
like OTP's scanner, constant patterns past it are illegal. OTP golden
`executables_integer_limit`.

<a id="step-28"></a>

### 28. Copy term graphs between heaps

Done 2026-10-06 (no OTP behavior). `ProcessHeap::add`/`Term::copy_to`
(`memory/copy`, `GraphCopy`): one iterative walk keyed by object address
copies a foreign same-runtime graph with its sharing kept (unlike ERTS's
flattening `copy_struct`) into one reservation; another runtime is
`wrong_owner`, stale/expired sources fail. Off-heap buffers are shared, charged
once runtime-wide (`detail::make_buffer` deleter) and once per process
(`HeapStorage::buffers_`). Factories admit inputs via `retain`. New
`runtime_copy` (every layout, shared towers, 100,000-deep lists, injected
exhaustion leaving both heaps unchanged); `runtime_lifecycle_failure`
`check_graph_copy`.

### G. Records, function values, dynamic calls

<a id="step-29"></a>

### 29. Lower record updates

Done 2026-10-07. OTP order: update values in source order, then the record,
then the shape check (`{badrecord, R}`, also `Expr#r{}`); `R#r{_ = V}` is
OTP's "meaningless use of _" error. `lowering_records` shares `check_record`
with access and copies only untouched fields. OTP golden
`executables_record_update`; records corpus row `rec_update_gate` accepted.

<a id="step-30"></a>

### 30. Implement `record_info/2`

Done 2026-10-07. Compile-time pseudo-function (`semantic::record_info_call`)
with erl_lint's messages (illegal record info, selector, tuple records only,
illegal in guards); `lower_record_info` emits the field list or size. OTP
golden `executables_record_info`.

<a id="step-31"></a>

### 31. Implement native, qualified and inferred record forms

Done 2026-10-07 through 31A–31E: all three OTP 29 forms share one
representation (contract `docs/native-records.md`). Not selected and still
unavailable: the `records` module, `RECORD_EXT`, upgrades.

<a id="step-31a"></a>

### 31A. Decide the native record contract

Done 2026-10-07 (decision). Error terms `{badrecord, X}`,
`{badrecord, {M, N}}` for failed external construction,
`{badfield, {{M, N}, F}}`, `{novalue, {{M, N}, F}}`; local/anonymous access
skip export checks, update and patterns do not; order tuple < native record <
map; `display` field order kept as definition order (difference).

<a id="step-31b"></a>

### 31B. Add native record cells and runtime services

Done 2026-10-07. ABI revision 6: `RecordDescriptor`,
`ModuleDescriptor::records`, `erlang_aot_record_v1` (make/get/update/match/
test with `RecordCheck` modes); `native_record` cell (header, untraced
`const RecordDefinition *`, values); walking, collection, copying, order and
printing. `runtime_records`.

<a id="step-31c"></a>

### 31C. Compile local native records

Done 2026-10-07. `-record #r{...}`, construction in source order, update
record-first, patterns (`record_test`/`record_field` plan nodes), guard
access, `is_record/2,3`; `ErrorReason::badfield` (20). OTP golden
`executables_native_records`. erlfmt cannot parse native record declarations
(fixture partly unformatted).

<a id="step-31d"></a>

### 31D. Compile qualified and imported native records

Done 2026-10-07. `-export_record`/`-import_record` with erl_lint messages,
`#m:r` forms resolved over `Module::peers`; external construction lowers the
defining module's literal defaults in place; `ErrorReason::novalue` (21). OTP
golden `executables_native_external`.

<a id="step-31e"></a>

### 31E. Compile anonymous native record forms

Done 2026-10-07. `X#_.f` (any native record), `X#_{...}` (record first,
exported or local), `#_{...}` patterns; `#_{...}` as an expression is OTP's
"native record '_' undefined". OTP golden `executables_native_anonymous`.
The `heap expressions` capability now covers only `compr_assign` and
undefined records.

<a id="step-32"></a>

### 32. Implement function values without captures

Done 2026-10-07 (contract `docs/funs.md`). ABI revision 7: `FunDescriptor`
table `<prefix>.funs`, `erlang_aot_make_fun_v1`, `erlang_aot_apply_v1`,
`ErrorReason` 22-24 (`badfun`, `badarity` with `{F, Args}`, `undef`);
`fun_closure` cells (untraced `FunDefinition *`, captured values). One value
per `fun f/1`; order atom < fun < tuple, local before external; local funs
print `#Fun<M.Index.0>` (difference). Calls go through the `erlang_aot.apply`
marker, which `lower_frames` turns into enter/tail transfers. OTP golden
`executables_fun_values`.

<a id="step-33"></a>

### 33. Implement closures with captured variables

Done 2026-10-07. Binding walker fun scopes (`FunScope`; heads shadow, nothing
leaks) record `Function::captures` in definition order (OTP's free-variable
order); lambdas compile to private `-f/A-fun-N-` functions taking arguments
then captures (`lower_lambda`); `lower_fun` roots captures. A record default
fun is one value for all construction sites (difference). OTP golden
`executables_closures` (20,000 closures across collections); new
`runtime_funs`.

<a id="step-34"></a>

### 34. Implement named funs

Done 2026-10-07. The name starts every clause as a new definition
(`FunScope::inside`, `Function::fun_names`, `FunEntry::self`), shadowing an
outer name, never captured, not visible after the fun; `name_self` builds the
fun on entry when read, so `Self =:= F`. `Name(...)` is a fun call (tail call
in constant stack). The `closures` capability is implemented. OTP golden
`executables_named_funs` (tail loops under `--max-stack 4096`).

<a id="step-35"></a>

### 35. Implement dynamic calls

Done 2026-10-07. ABI revision 8: `ExportDescriptor::frame`; registration binds
module/export atoms (`ModuleAtoms::module/exports`); services
`erlang_aot_call_v1` (`M:F(Args)`), `erlang_aot_apply_list_v1`/
`erlang_aot_call_list_v1` (`apply/2,3`, list unpacked into the registers),
`erlang_aot_make_external_fun_v1` (runtime `fun M:F/A`, definitions interned
by `CodeServer::external_fun`). OTP order and errors: module, function,
arguments; non-atom names, improper lists and bad arities `badarg`, missing
exports `undef`, more than 255 arguments `undef`/`badarity`. Dynamic calls
share the fun-call transfer (tail calls in tail position). Builtins reached
dynamically raise `undef` and builtin funs keep the `dynamic calls` capability
until step 36; the `undef` top frame differs from OTP. Lookups scan modules
linearly (hash maps: step 62A). OTP golden `executables_dynamic_calls`.

## H. Builtins and libraries

<a id="step-36"></a>

### 36. Implement the generic production builtin bridge

Backlog: F26. Depends on: [11](#step-11).

Register production builtins by module/name/arity and call them from generated
code with checked status and owned results.

- Success criteria
  - [x] Generated code calls a registered builtin; unregistered names keep the
    unavailable diagnostic; failures use the checked error channel.
- Tests
  - [x] Golden programs calling bridge builtins with valid and invalid
    arguments.
  - [x] Focused test for duplicate registration and failure rollback.
- Evidence (2026-10-07): `maint-29` unchanged at `21776803`. Contract
  `docs/builtins.md`. ABI: append-only catalog `abi::v1::bridge_builtins`
  (72 `erlang` entries: guard BIFs, operators, `display/1`, `halt/0,1`, the
  raise family, `function_exported/3`) and service `erlang_aot_builtin_v1`
  (index, arguments, output); no descriptor revision change. Runtime:
  `BuiltinRegistry` in the `CodeServer` (all-or-none batches; invalid,
  duplicate and in-batch duplicate entries and allocation failures roll
  back), `erlang_builtins()` registered at startup as adapters over the
  inline services; `BuiltinFrame` = `FrameDescriptor` with a null body that
  `ProcessStack::enter` runs on the registers without a push;
  `CodeServer::function_frame` (exports, then builtins) serves `M:F(Args)`,
  `apply/3` and runtime `fun M:F/A`; registration binds external funs without
  a frame to builtins. Compiler: catalog builtins without an inline service
  are body builtins (`ServiceResolution::builtin`, `lower_builtin`); `fun F/A`
  of an auto-imported catalog builtin becomes `erlang:F/A`
  (`Function::builtin_funs`, `add_builtin_fun`); `halt/0,1` auto-imported;
  other `erlang` names keep `unknown module erlang` / `dynamic calls`
  (catalog owner now step 52). OTP golden `executables_builtin_bridge` (every
  catalog builtin through `apply/3` with valid arguments, 46 error cases with
  OTP classes/reasons, builtin funs: equality, printing, higher-order use,
  runtime `fun M:F/A`, badarity; `function_exported/3`; `halt` through apply,
  unqualified `halt()`, uncaught error through apply) passes all 8
  combinations; `runtime_builtins` registry checks; `runtime_lifecycle_failure`
  startup sweep raised to 256 allocations (registration rollback on every
  failpoint); mangling spellings checked with Clang on four targets; semantic
  cases. Difference recorded: `function_exported/3` is true only for the
  builtins this runtime provides. Fresh Windows x64 Debug (clang-cl): fast
  174/174, full `-j 12` 178/178; after complexity splits Lizard 0 warnings and
  tidy 291 units pass (rerun with one job after clang-tidy crashed with two).
  Logs `build/plan11-step36/`.

<a id="step-37"></a>

### 37. Add the term-access builtin family

Backlog: F26. Depends on: [36](#step-36).

Potentially long running functions will need to be able to do work in interruptible portions to allow the scheduler on the same cpu core to switch to other tasks (continuations sort of thing)

`element/2`, `setelement/3`, `tuple_size/1`, `make_tuple/2,3`,
`tuple_to_list/1`, `list_to_tuple/1`, `hd/1`, `tl/1`, `length/1`, `map_get/2`,
`map_size/1` and `is_map_key/2` in body context, plus the list operators
`++`/`--` (`erlang:'++'/2`, `erlang:'--'/2`).

- Success criteria
  - [x] Results and `badarg` errors match OTP.
- Tests
  - [x] Golden call/result corpus regenerated from OTP with boundary and invalid
    arguments.
- Evidence (2026-10-07): `maint-29` unchanged at `21776803`. New bridge entries
  (catalog appended) in `runtime/src/builtins/term_access.cpp`
  (`term_access_builtins()`, registered through `production_builtins()`):
  `setelement/3`, `make_tuple/2,3`, `tuple_to_list/1`, `list_to_tuple/1`,
  `'++'/2`, `'--'/2` with OTP's `bif.c`/`erl_bif_lists.c` rules (`--` by exact
  order, O((n + m) log m)); shared helpers `builtins/support.hpp`. `A ++ B` and
  `A -- B` lower to the bridge, so the `arithmetic` capability is implemented
  (removed from `codegen_placeholders`). Auto-import follows
  `erl_internal:bif/2`: `setelement`, `tuple_to_list`, `list_to_tuple` yes,
  `make_tuple` no. `element`, `tuple_size`, `hd`, `tl`, `length`, `map_get`,
  `map_size`, `is_map_key` already ran in bodies (inline services, bridge since
  step 36). Interruptibility (user note): every builtin still runs to
  completion; `TODO(step 43A)` markers and new plan step 43A convert them once
  the scheduler exists. OTP golden `executables_term_access` (76 apply/3 cases
  with boundary and invalid arguments, direct and qualified calls, operator
  evaluation order, builtin funs, 100,000-element `++`/`--`/tuples) passes all
  8 combinations; semantic cases; textstats diagnostics refreshed (now stops
  at `lists`/`io`). Fresh Windows x64 Debug (clang-cl): fast 175/175, full
  `-j 12` 179/179; Lizard 0 warnings, tidy pass after replacing a constexpr
  optional dereference (analyzer false positive). Logs `build/plan11-step37/`.

<a id="step-38"></a>

### 38. Add the conversion builtin family

Backlog: F11, F26. Depends on: [36](#step-36).

Potentially long running functions will need to be able to do work in interruptible portions to allow the scheduler on the same cpu core to switch to other tasks (continuations sort of thing)

Atom, integer, float, list, binary and string conversions
(`atom_to_list/1`, `list_to_atom/1`, `integer_to_list/1,2`,
`list_to_integer/1,2`, `float_to_list/1,2`, `binary_to_list/1`,
`list_to_binary/1`, `iolist_to_binary/1`, `term_to_binary/1` excluded unless
selected).

- Success criteria
  - [x] Results and errors match OTP; atom-table limits fail as documented.
- Tests
  - [x] Golden call/result corpus regenerated from OTP.
- Evidence (2026-10-07): `maint-29` unchanged at `21776803`. Catalog appended;
  `runtime/src/builtins/conversions.cpp` (`conversion_builtins()`) and
  `float_text` implement `atom_to_list/1`, `list_to_atom/1`,
  `integer_to_list/1,2`, `list_to_integer/1,2`, `float_to_list/1,2`,
  `binary_to_list/1`, `list_to_binary/1`, `iolist_to_binary/1` from OTP's
  `bif.c`, `big.c`, `utils.c`, `erlang.erl` rules and probes (OTP 29.1.1):
  255-character atoms with the length checked first (`system_limit`),
  bases 2..36, OTP's list_to_integer size limits ahead of digit checks,
  `float_to_list` default `%.20e`, `{scientific, D}` (negative is 6),
  `{decimals, D}` with OTP's own rounding and `compact` (including its
  integer-zero trimming), `short` via `std::to_chars` digits placed by OTP's
  Ryu rules, 256-byte text limit; iolists walked iteratively. A full atom
  table is a `resource_limit` runtime failure (exit 70); differences recorded
  for that and for `list_to_integer` characters above 255. All auto-imported.
  OTP golden `executables_conversions` (124 apply/3 cases, direct calls, funs,
  round trips, 1000-digit integers in bases 2/10/36, a 100,000-deep iolist,
  `system_limit`/`badarg` for 1,300,000-digit strings; `atoms` run, authored
  `--max-atoms 300` run exiting 70) passes all 8 combinations; semantic case;
  `textstats`/`frames` diagnostics refreshed. Fresh Windows x64 Debug: fast
  176/176, full `-j 12` 180/180; Lizard 0 warnings and tidy pass after
  complexity and swappable-parameter splits and a boost analyzer workaround
  (`swap` instead of copy assignment). Logs `build/plan11-step38/`.

<a id="step-39"></a>

### 39. Ship a project-owned library subset for `lists` and `maps`

Backlog: F08, F26. Depends on: [21](#step-21), [35](#step-35).

Potentially long running functions will need to be able to do work in interruptible portions to allow the scheduler on the same cpu core to switch to other tasks (continuations sort of thing)

Write original Erlang implementations (not OTP copies) of commonly used
functions, compiled and linked with user programs. Start with `lists:reverse,
map, foldl, foldr, filter, member, keyfind, sort, seq, nth, append` and
`maps:get, put, find, keys, values, fold, from_list, to_list`.

- Success criteria
  - [x] Library modules build with the compiler and link automatically into
    executables; results match OTP.
- Tests
  - [x] Golden call/result corpus regenerated from OTP for every function.
- Evidence (2026-10-07): `maint-29` unchanged at `21776803`. Original Erlang
  sources `library/stdlib/lists.erl` (`append/1,2`, `filter/2`, `foldl/3`,
  `foldr/3`, `keyfind/3`, `map/2`, `member/2`, `nth/2`, `reverse/1,2`,
  `seq/2,3`, `sort/1`) and `maps.erl` (`find/2`, `fold/3`, `from_list/1`,
  `get/2`, `keys/1`, `put/3`, `to_list/1`, `values/1`), written from OTP's
  documented behavior with its error shapes (OTP probes: `map`/`foldl`
  `{case_clause, X}` for a non-list, `nth` `is_integer` guard, `seq/3`
  `badarg`); contract `docs/library.md`. The driver adds them to a batch
  (`frontend` `add_library`, positional and project) when a module names them
  with a literal atom (`semantic::referenced_modules`) and no input declares
  them; the directory is `library/stdlib` relative to `erlangaot`
  (`linking::library_directory`, `ERLANG_AOT_DEFAULT_LIBRARY`). Interruptible
  by construction (Erlang code). Differences recorded: subset only, key-order
  iteration, no code-path loading for runtime-only names. OTP golden
  `executables_library` (101 apply/3 cases with valid, boundary and invalid
  arguments, fun application order, library funs/apply/dynamic calls, a
  32-key `from_list`/`to_list` round trip, 10,000-element lists) passes all 8
  combinations; program diagnostics refreshed (`lists`/`maps` resolve). Fresh
  Windows x64 Debug: fast 177/177, full `-j 12` 181/181; Lizard 0 warnings;
  tidy 294 units pass (rerun with one job after a clang-tidy crash). Logs
  `build/plan11-step39/`.

<a id="step-40"></a>

### 40. Add console output through `io`

Backlog: F26. Depends on: [4](#step-4), [36](#step-36).

`io:put_chars/1`, `io:format/1,2` with `~w ~p ~s ~n ~b ~B ~c ~~`.

- Success criteria
  - [x] Output matches OTP for the supported directives; unsupported directives
    and bad arguments raise OTP-like errors.
- Tests
  - [x] Golden programs printing each directive, nested terms with `~p` line
    breaking, and Unicode strings.
- Evidence (2026-10-07): `maint-29` unchanged at `21776803`. Contract
  `docs/io.md`. Catalog appended with module `io` (`format/1,2`,
  `put_chars/1`); the compiler resolves qualified calls of another module's
  catalog builtin as services (`semantic::module_builtin`), so `io:format`,
  `fun io:format/2`, `apply(io, ...)` and `M:format` reach the bridge.
  Runtime `builtins/io_format` (OTP `io_lib_format` scan and control
  sequences `~w ~p ~s ~c ~b ~B ~i ~n ~~` with width, precision, pad, `*`,
  `t`/`l`/`k`, column tracking with tabs, list formats passing nested
  chardata through), `builtins/io_pretty` (OTP `io_lib_pretty` intermediate
  form and pp/cind layout: tagged tuples, maps, native records, binaries
  wrapping, improper tails; printable range latin1), shared `builtins/text`
  (UTF-8, digits, from `conversions`); output is the device's UTF-8
  (`unicode` encoding), nothing written on error; `TermStyle::write_unicode`
  for `~tw` atoms. Differences recorded: `~e ~f ~g ~x ~X ~+ ~# ~W ~P` and `K`
  are badarg; `~p` nesting over 256 is `system_limit` (layout recursion,
  about 1.2 KiB per level in Debug, 800 levels overflowed); widths count code
  points; negative counts badarg (OTP loops). OTP golden
  `executables_console` (every directive with fields, 32 error cases, 23
  `~p` layouts incl. columns, widths, precisions, tabs, binaries, nested
  maps; Unicode `~ts`/`~tp`/`~tc`/atoms/binary formats; put_chars chardata
  and 10 errors; funs and dynamic calls; 10,000-element `~w`, 2,000-element
  `~p`; a depth-256 `~0p` run; authored runs for unsupported directives and
  the depth-257 `system_limit`) passes all 8 combinations; semantic cases
  `io_builtins`, `io_unknown`, `io_guard`. Program diagnostics refreshed:
  `avltree`, `frames`, `textstats` now compile, and their linked executables
  reproduce their OTP stdout and exit status (run by step 58). Fresh Windows
  x64 Debug: fast 178/178, full `-j 12` 182/182; Lizard 0 warnings; tidy
  findings (complexity splits, swappable parameters) fixed and rerun clean.
  Logs `build/plan11-step40/`.

<a id="step-41"></a>

### 41. Add typed native callables for builtin implementations

Backlog: F27. Depends on: [37](#step-37), [38](#step-38).

Use the builtin families as the concrete use case: typed C++ signatures with
checked argument conversion and generic Term fallback.

- Success criteria
  - [x] Existing builtins migrate to typed wrappers with identical behavior;
    wrong types become `badarg`; C++ exceptions never cross the generated ABI.
- Tests
  - [x] Existing builtin goldens pass unchanged.
  - [x] Focused tests for conversion failure, expired handles and a throwing
    callback.
- Evidence (2026-10-07): no OTP-dependent change. `runtime/src/builtins/typed.hpp`:
  `typed<Function>` adapts `Result(ProcessContext &, Parameters...)` to a
  `BuiltinBody`; `typed_entry<Function>(module, name)` takes the arity from
  the signature. Arguments are admitted in order (unowned words are
  failures), then converted by `Argument<T>` (`Term` fallback,
  `std::int64_t` small, `detail::Integer`, `double`, `ListArgument`,
  `TupleArgument`, `BinaryArgument`, `AtomArgument`); a mismatch raises
  badarg without running the body. Results `Term`, `TermResult<Term>`,
  `BuiltinResult<Term>` or a self-published `Word`; a thrown
  `BuiltinFailure` (moved from io to `support.hpp`) is raised or recorded,
  other exceptions stay in `call_builtin`. Migrated: term-access family,
  conversions, io, `binary_part/2`, `function_exported/3`; the remaining
  erlang adapters forward raw words to the inline services (documented in
  `docs/builtins.md#typed-builtins`). Unchanged goldens
  (`builtin_bridge`, `term_access`, `conversions`, `console`, `library`)
  pass; new `runtime_typed_builtins` (conversion failures of every argument
  type skip the body; foreign and destroyed-process words are service
  failures; runtime_error, bad_alloc and thrown BuiltinFailure become
  internal_error, out_of_memory, system_limit and the term status). Fresh
  Windows x64 Debug: fast 179/179, full `-j 12` 183/183; Lizard 0 warnings;
  tidy (now batched) flagged one swappable-parameter pair in `'++'/2`,
  fixed (`ListArgument` left operand) and the affected shards rerun clean.
  Logs `build/plan11-step41/`.

## I. Processes and messaging

<a id="step-42"></a>

### 42. Implement pid and reference identities

Backlog: F03, F07, F12. Depends on: [23](#step-23), [28](#step-28).

`self/0` (main process only) and `make_ref/0`, with comparison, printing,
tracing and copying.

- Success criteria
  - [x] Identities are unique, compare and print like OTP; forged or stale
    words are rejected.
- Tests
  - [x] Golden programs comparing, sorting and storing pids/references in maps.
  - [x] Runtime tests for forged, stale and foreign identities.
- Evidence (2026-10-08): `maint-29` unchanged at `21776803`. Contract
  `docs/terms.md#pids-and-references`. Pids are immediates (tag `0x3`)
  carrying a process number from one process-wide, never-reused sequence;
  `detail::ProcessNumbers` (`process/identities`) records each runtime's
  issued numbers as runs and `ProcessIdentity` carries the number. Admission
  (`TermAccess::pid` via `HeapStorage::processes_`, heap verify) rejects
  forged and foreign pid words; exited pids stay valid. References are
  `reference` heap cells (untraced 64-bit number from a program-wide
  counter): walked, collected and copied like other untraced cells. Order
  numbers < atoms < references < funs < pids < tuples (pids and references by
  number); printing `<0.N.S>` and `#Ref<0.A.B.C>` with OTP's bit splits.
  Catalog appended: `self/0`, `make_ref/0` (`builtins/processes`),
  `pid_to_list/1`, `ref_to_list/1` (conversions), all auto-imported; a body
  `self()` resolves its guard signature to the bridge builtin, `self()` in a
  guard stays gated (step 52); `is_pid/1`/`is_reference/1` now return true
  for these values. Differences recorded: pid and reference numbering. OTP
  golden `executables_identities` (type tests, equality, head matching,
  identity text shapes via `pid_to_list`/`ref_to_list`, their badarg cases,
  term order of a mixed list, 200 distinct sorted references as map keys,
  pid/reference/compound map keys, captures, exception payloads) passes all
  8 combinations; `runtime_identities` (issued, forged, never-issued,
  foreign-runtime and exited pids; references across collection, copy,
  stale and expired terms; order); semantic cases `process_identities`,
  `self_guard`; ring/supervise diagnostics lose their `self()` guards line.
  The deferred term-services path moved to `TermFactory::port`
  (`PortIdentity` placeholder). Fresh Windows x64 Debug: fast 181/181, full
  `-j 12` 185/185; Lizard 0 warnings after splitting the walker's payload
  check; tidy 302 units pass. Logs `build/plan11-step42/`.

<a id="step-43"></a>

### 43. Run spawned processes on a cooperative executor

Backlog: F01, F22. Depends on: [42](#step-42), [33](#step-33).

Implement `spawn/1,3` with per-process heaps, reduction counting and yields per
the step-17 model; startup runs the entry as the first process and exits when
it finishes.

- Success criteria
  - [x] Many processes interleave on one thread; each heap is isolated.
  - [x] A crashing process does not affect others.
- Tests
  - [x] Golden programs spawning 10k processes and long-running busy loops
    that must interleave.
  - [x] Teardown with live processes releases every heap.
- Evidence (2026-10-08): `maint-29` unchanged at `21776803`. Contract
  `docs/processes.md`. Yields per the step-17 model: every function entry
  (`ProcessStack::enter`, so calls, tail calls, fun/dynamic calls and
  builtins) spends a reduction of a 4,000-reduction slice; at zero it records
  the entered frame (`resume_`), keeps its argument registers as roots and
  returns code that ends the slice, unwinding the `musttail` chain;
  `ProcessStack::start`/`run` begin and resume a process; host `invoke`
  resumes its own yields. `detail::Executor` (`scheduler/executor`, in
  `Runtime::Impl`): FIFO run queue, round-robin slices until the main
  process ends or another process halts or fails outside Erlang; ended
  processes are released at once (contexts now keyed by pointer, live
  processes by pid number). Catalog appended: `spawn/1,3`,
  `is_process_alive/1` (auto-imported); spawn checks its arguments in the
  parent (badarg), copies the fun or argument list into the new heap and
  prepares the first call inside the child with the dynamic call services,
  so badarity/undef crash only the child. Startup queues the entry frame as
  the main process (host-only entry exports still run synchronously) and
  releases every process at exit. Crash reports stay silent until step 44;
  loops without calls and long builtins do not yield (step 43A). Difference
  recorded: OTP's compiler may drop the code after `spawn` of a fun of
  another arity. OTP golden `executables_processes` (a process spawned
  first spinning on `is_process_alive/1` until a later one ends, 10,000
  processes, captured and argument copies, spawn/is_process_alive badarg
  cases, an endless process left running at exit; a `crash` run with an
  exception, undef and badarity child beside a surviving main) passes all 8
  combinations; `runtime_processes` (round-robin slices of spinning
  processes, crash isolation, halt from another process, host-invocation
  yields, releasing live processes returns all memory); semantic case
  `spawn_builtins`. Fresh Windows x64 Debug: fast 183/183, full `-j 12`
  187/187; Lizard 0 warnings; tidy 302 units pass after replacing two
  swappable-parameter pairs (`InitialCall`, `entry_frame(startup)`). Logs
  `build/plan11-step43/`.

<a id="step-43a"></a>

### 43A. Make long-running builtins interruptible

Backlog: F22, F26. Depends on: [43](#step-43). Added 2026-10-07 during step 37.

Builtins whose work grows with their input run to completion today because no
scheduler exists. Once reductions and yields exist, give them BEAM-style traps:
a builtin does a bounded portion of work, keeps its state rooted (registers or
a heap/off-heap state term), and continues through its builtin frame after the
scheduler may have switched processes. Candidates are marked `TODO(step 43A)`
in `runtime/src/builtins/`: `'++'/2`, `'--'/2`, `list_to_tuple/1`,
`tuple_to_list/1`, `make_tuple/2,3`, `setelement/3`, the step-38 conversions,
plus the inline `length/1` and long list services.

- Success criteria
  - [x] A process running a long builtin cannot starve others; results, errors
    and evaluation order stay as before; state survives collections between
    portions.
- Tests
  - [x] Existing builtin goldens pass unchanged; an interleaving golden runs a
    huge `++`/`--` beside a busy process; a collection between portions keeps
    the state.
- Evidence (2026-10-08): `maint-29` unchanged at `21776803`. Contract
  `docs/builtins.md#portions`. Body bridge builtins are entered like functions
  (`erlang_aot_builtin_frame_v1` replaces `erlang_aot_builtin_v1`; the apply
  marker transfer), so each spends a reduction; body `length/1` resolves to
  its bridge builtin, guards keep the inline service. A portion may do 16
  units of work per reduction left (`ProcessStack::budget`/`spend`), then
  `trap`s to a continuation frame with state terms in the registers; `enter`
  suspends the process there and later resumes it. Native state lives in a
  `TrapState` whose words are roots. In portions: `length/1`, `++` (collect,
  then build onto the tail), `--` (collect, bottom-up merge sort by exact
  order, binary-search scan charged per comparison, build; nothing removed
  returns the left list), `binary_to_list/1`, `list_to_binary/1`,
  `iolist_to_binary/1` (`builtins/lists`, `builtins/portions`,
  `TermFactory::list_words`). As in OTP the tuple builtins and the bounded
  conversions run to completion; io formatting also does (difference
  recorded). Host `call_builtin` continues traps at once. OTP golden
  `executables_portions` (each builtin beside a busy process that ends during
  it, results, late improper-tail badargs) passes; `runtime_portions` runs
  each as a process's first call with a collection between every two portions
  (++/length/binary_to_list/list_to_binary over moving tuples, -- sorting and
  scanning in more than four portions); all builtin goldens unchanged. Fresh
  Windows x64 Debug: fast 185/185; full `-j 12` 188/189, the one failure a
  `runtime_containers` SegFault not reproduced in 13 standalone or parallel
  reruns (it passed in the same gate's fast run); Lizard 0 warnings after
  extracting `guard_service`; tidy 39 batches pass after splitting the merge
  (`close_runs`). Logs `build/plan11-step43a/`.

<a id="step-44"></a>

### 44. Define process exit and crash reports

Backlog: F22. Depends on: [43](#step-43).

- Success criteria
  - [x] Normal return, `exit/1` and uncaught errors terminate the process with
    OTP-like reasons; non-normal termination writes an error report.
- Tests
  - [x] Golden programs for each termination kind with stderr checked.
- Evidence (2026-10-08): `maint-29` unchanged at `21776803`. Contract
  `docs/processes.md#exits`. `process/exits`: `exit_reason` gives `normal`,
  the `exit/1` reason, `{Reason, Stack}` for errors and
  `{{nocatch, V}, Stack}` for uncaught throws (OTP); `report_exit` writes
  OTP's legacy report (`=ERROR REPORT==== D-Mon-YYYY::HH:MM:SS.uuuuuu ===`,
  `Error in process <pid> with exit value:`, the reason as `~p`, blank line)
  on stderr for error-class ends only, when the executor releases a non-main
  process. OTP probes: no report for `exit/1` (incl. `normal`, `kill`), reports
  for errors, `{nocatch, V}` and `undef`; OTP's logger writes them
  asynchronously, so its oracle runs usually show none. Difference recorded
  (report timing and stream). OTP golden `executables_crash_reports` (return,
  exit normal/shutdown/kill/term, caught error, error, throw, badarith,
  badmatch, undef via spawn/3; authored stderr pattern for exactly the five
  reports); `executables_processes` crash run now expects its three reports.
  Gate (after the gate-time commit): fast 189/189 in 46 s, full 193/193 in
  106 s (separate check), check-quality all 306 units in 2 min 13 s (16
  jobs) after replacing an empty catch. Logs `build/plan11-step44/`.

<a id="step-45"></a>

### 45. Implement the signal inbox and message send

Backlog: F05, F24. Depends on: [43](#step-43), [28](#step-28).

`Pid ! Msg` and `erlang:send/2`; every message, including self-send, enters the
signal inbox and is copied by the step-28 service into a heap fragment of the
receiver, merged into its heap at the next collection.

- Success criteria
  - [x] Per-sender order is preserved; sending to a dead process succeeds
    silently; invalid destinations raise `badarg`.
- Tests
  - [x] Golden programs for ping-pong, fan-in ordering and self-send: the
    send semantics golden here; ping-pong and fan-in need receive and are
    goldens of step 46.
  - [x] Copy failure in the receiver is handled per contract.
- Evidence (2026-10-08): `maint-29` unchanged at `21776803`. Contract
  `docs/processes.md#messages`. Catalog appended `'!'/2` and `send/2`
  (`send/2` not auto-imported); `A ! B` lowers to the `!` bridge builtin
  (destination first, value is the message); `send expressions` and runtime
  `message passing` features implemented (the `ProcessContext::send`
  placeholder and its deferred-report tests removed). `Executor::send` copies
  the message into the live receiver's heap (`Term::copy_to`, step 28) and
  appends it to the signal inbox of the concrete `Mailbox` (inbox + queue +
  saved position: `peek`/`skip`/`take`/`restart`, arrived messages join the
  queue on `peek`); messages are roots (`visit_roots`). Ended pid: nothing;
  `{Atom, Atom}`: dropped (no registry until step 50); other destinations
  badarg; a refused copy fails the sender (runtime failure, exit 70). OTP
  probes: Dest before Msg, `{name, nonode@nohost}`/remote node silent, bare
  unregistered atom and `{name, 1}` badarg. OTP golden `executables_send`
  (order of evaluation, return values, large messages to a busy process, dead
  pid, self-send, `erlang:'!'/2`, every destination kind); `runtime_messages`
  (per-sender order mixed with self-sends across a receiver collection,
  receive positions and arrivals, ended receiver, a receiver heap cap refusing
  the copy and delivering nothing, teardown releases everything);
  programs kvstore/ring/supervise lose their send diagnostics. Fresh Windows
  x64 Debug: fast 191/191 (62 s); check-quality (Lizard, tidy 305 units in 5
  batches) passes. Logs `build/plan11-step45/`.

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

<a id="step-62a"></a>

### 62A. Index code server lookups with hash maps

Backlog: F33. Depends on: [35](#step-35); independent of the other phase L
steps, so it may move earlier. Added 2026-10-07 after step 35.

`CodeServer::export_frame` (dynamic calls, runtime `fun M:F/A`) scans every
registered module and then its export list; `fun_definition`,
`record_definition` and `atom_word` scan modules by descriptor. Replace the
scans with hash maps built at registration: module atom word to its module,
`(function atom, arity)` to the export frame, and descriptor address to its
bindings.

- Success criteria
  - [ ] Dynamic call, apply/3 and runtime `fun M:F/A` lookups take constant
    expected time in the number of modules and exports; descriptor lookups
    likewise.
  - [ ] Maps are built inside the registration transaction (a failed
    registration publishes none of them) and stay valid while modules stay
    registered; when the code server becomes concurrent (phase J), lookups
    stay safe under its synchronization.
- Tests
  - [ ] Existing goldens (`executables_dynamic_calls`, `runtime_funs`) pass
    unchanged; a focused runtime test registers many modules with many
    exports and checks lookups of present, missing and wrong-arity names.
  - [ ] Lookup cost with many modules recorded descriptively, not gated.

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
