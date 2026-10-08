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
oracle OTP 29.1.1 / ERTS 17.1. Latest combined Windows x64 Debug gate (phase J close, step 57,
2026-10-08): 209 full-mode CTests and 311 production quality units.

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
| J2. Ports and port I/O | [57A](#step-57a)–[57F](#step-57f) | F07, F23, F26, F35 |
| K. End-to-end projects | [58](#step-58) | F01, V03 |
| L. Optimization and tooling | [58A](#step-58a)–[58H](#step-58h), [59](#step-59)–[62](#step-62), [62A](#step-62a), [62B](#step-62b) | F23, F25, F29–F34 |
| M. Validation closure | [63](#step-63)–[70](#step-70) | V01–V04 |
| N. Optional scope decisions | [71](#step-71)–[77](#step-77) | D01–D07 |
| O. Final closure | [78A](#step-78a), [78](#step-78) | all |

---

## Completed steps 1–47 (compact record)

Full step texts, criteria and per-step evidence are in Git history (last full
versions: steps 1–8G at `a4e07bb`, steps 8H–27E at `9decf7a`, steps 28–35 at
`c82066f`, steps 36–47 at `b0e0f63`). Every step below passed the common gate;
per-step logs are in `build/plan11-step*/`.

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

### H. Builtins and libraries

<a id="step-36"></a>

### 36. Implement the generic production builtin bridge

Done 2026-10-07 (contract `docs/builtins.md`). Append-only catalog
`abi::v1::bridge_builtins` (first 72 `erlang` entries: guard BIFs, operators,
`display/1`, `halt/0,1`, raise family, `function_exported/3`). Runtime
`BuiltinRegistry` in the `CodeServer` (all-or-none batches, rollback on
invalid/duplicate entries and allocation failure); `BuiltinFrame` is a
`FrameDescriptor` with a null body run on the registers;
`CodeServer::function_frame` (exports, then builtins) serves `M:F(Args)`,
`apply/3` and runtime `fun M:F/A`. Compiler: catalog builtins without an inline
service are body builtins (`lower_builtin`); `fun F/A` of an auto-imported
builtin becomes `erlang:F/A` (`Function::builtin_funs`). Difference:
`function_exported/3` is true only for provided builtins. OTP golden
`executables_builtin_bridge`; `runtime_builtins`; startup failure sweep 256
allocations.

<a id="step-37"></a>

### 37. Add the term-access builtin family

Done 2026-10-07. `builtins/term_access.cpp`: `setelement/3`, `make_tuple/2,3`,
`tuple_to_list/1`, `list_to_tuple/1`, `'++'/2`, `'--'/2` by OTP's
`bif.c`/`erl_bif_lists.c` rules (`--` by exact order, O((n + m) log m));
helpers `builtins/support.hpp`. `++`/`--` lower to the bridge (`arithmetic`
capability implemented). Auto-import follows `erl_internal:bif/2`
(`make_tuple` not). The other listed BIFs already ran through inline services.
OTP golden `executables_term_access`.

<a id="step-38"></a>

### 38. Add the conversion builtin family

Done 2026-10-07. `builtins/conversions.cpp` and `float_text`: atom, integer,
float, list and binary conversions by OTP's `bif.c`/`big.c`/`utils.c` rules:
255-character atoms (length checked first, `system_limit`), bases 2..36,
`float_to_list` default `%.20e`, `scientific`/`decimals`/`compact`/`short`
(`std::to_chars` digits placed by OTP's Ryu rules), 256-byte text limit;
iolists walked iteratively. A full atom table is a `resource_limit` failure
(exit 70); differences recorded for that and `list_to_integer` characters above
255. OTP golden `executables_conversions` (with a `--max-atoms 300` run).

<a id="step-39"></a>

### 39. Ship a project-owned library subset for `lists` and `maps`

Done 2026-10-07 (contract `docs/library.md`). Original
`library/stdlib/lists.erl` and `maps.erl` subsets with OTP's error shapes. The
driver adds a library module to the batch when a module names it with a
literal atom (`semantic::referenced_modules`, `frontend` `add_library`) and no
input declares it; directory `library/stdlib` relative to `erlangaot`
(`linking::library_directory`, `ERLANG_AOT_DEFAULT_LIBRARY`). Differences:
subset only, key-order iteration, no code-path loading. OTP golden
`executables_library`.

<a id="step-40"></a>

### 40. Add console output through `io`

Done 2026-10-07 (contract `docs/io.md`). Catalog module `io` (`format/1,2`,
`put_chars/1`); qualified calls of another module's catalog builtin resolve as
services (`semantic::module_builtin`). Runtime `builtins/io_format`
(`io_lib_format` directives `~w ~p ~s ~c ~b ~B ~i ~n ~~` with width,
precision, pad, `*`, `t`/`l`/`k`), `builtins/io_pretty` (`io_lib_pretty`
layout), `builtins/text`; UTF-8 output, nothing written on error. Differences:
`~e ~f ~g ~x ~X ~+ ~# ~W ~P` and `K` are badarg, `~p` nesting over 256 is
`system_limit`, widths count code points, negative counts badarg. OTP golden
`executables_console`; `avltree`, `frames`, `textstats` now compile.

<a id="step-41"></a>

### 41. Add typed native callables for builtin implementations

Done 2026-10-07 (`docs/builtins.md#typed-builtins`). `builtins/typed.hpp`:
`typed<Function>` adapts `Result(ProcessContext &, Parameters...)` to a
`BuiltinBody`, `typed_entry` takes the arity from the signature;
`Argument<T>` conversions (`Term`, `std::int64_t`, `detail::Integer`,
`double`, `List`/`Tuple`/`Binary`/`AtomArgument`) raise badarg before the body;
a thrown `BuiltinFailure` is raised, other exceptions stay in `call_builtin`.
Migrated: term access, conversions, io, `binary_part/2`,
`function_exported/3`; other erlang adapters forward raw words.
`runtime_typed_builtins`.

### I. Processes and messaging (steps 42–47)

<a id="step-42"></a>

### 42. Implement pid and reference identities

Done 2026-10-08 (contract `docs/terms.md#pids-and-references`). Pids are
immediates (tag `0x3`) with a never-reused process number
(`detail::ProcessNumbers`, `process/identities`); admission rejects forged and
foreign pid words, exited pids stay valid. References are untraced `reference`
heap cells numbered program-wide. Order numbers < atoms < references < funs <
pids < tuples; printing `<0.N.S>`, `#Ref<0.A.B.C>`. Builtins `self/0`,
`make_ref/0` (`builtins/processes`), `pid_to_list/1`, `ref_to_list/1`; guard
`self()` gated until step 52; ports moved to `TermFactory::port`
(`PortIdentity` placeholder). Difference: numbering. OTP golden
`executables_identities`; `runtime_identities`.

<a id="step-43"></a>

### 43. Run spawned processes on a cooperative executor

Done 2026-10-08 (contract `docs/processes.md`). Every function entry
(`ProcessStack::enter`) spends a reduction of a 4,000 slice; at zero it records
`resume_`, roots the argument registers and ends the slice by unwinding the
`musttail` chain (`ProcessStack::start`/`run`). `detail::Executor`
(`scheduler/executor`): FIFO run queue, round-robin slices until the main
process ends or another halts or fails; ended processes are released at once.
`spawn/1,3`, `is_process_alive/1`: badarg checked in the parent, fun/arguments
copied into the new heap, first call prepared in the child (badarity/undef
crash only the child). Startup runs the entry as the main process. Difference:
OTP may drop code after `spawn` of a fun of another arity. OTP golden
`executables_processes`; `runtime_processes`.

<a id="step-43a"></a>

### 43A. Make long-running builtins interruptible

Done 2026-10-08 (contract `docs/builtins.md#portions`). Body bridge builtins
are entered like functions (`erlang_aot_builtin_frame_v1` replaced
`erlang_aot_builtin_v1`) and spend a reduction; body `length/1` uses the
bridge, guards the inline service. A portion does 16 work units per reduction
left (`ProcessStack::budget`/`spend`), then `trap`s to a continuation frame
with state in registers or a rooted `TrapState`. In portions: `length/1`,
`++`, `--` (merge sort by exact order, binary-search scan),
`binary_to_list/1`, `list_to_binary/1`, `iolist_to_binary/1`
(`builtins/lists`, `builtins/portions`). Tuple builtins and bounded
conversions run to completion as in OTP; io formatting too (difference). OTP
golden `executables_portions`; `runtime_portions` (collection between
portions). One `runtime_containers` SegFault in that full gate was not
reproduced.

<a id="step-44"></a>

### 44. Define process exit and crash reports

Done 2026-10-08 (contract `docs/processes.md#exits`). `process/exits`:
`exit_reason` gives `normal`, the `exit/1` reason, `{Reason, Stack}` for errors
and `{{nocatch, V}, Stack}` for throws; `report_exit` writes OTP's legacy
`=ERROR REPORT====` text to stderr for error-class ends of non-main processes
only. Difference: report timing and stream (OTP's logger is asynchronous). OTP
golden `executables_crash_reports`.

<a id="step-45"></a>

### 45. Implement the signal inbox and message send

Done 2026-10-08 (contract `docs/processes.md#messages`). Builtins `'!'/2` and
`send/2` (not auto-imported); `A ! B` lowers to `!` (destination first).
`Executor::send` copies into the live receiver's heap (`Term::copy_to`) and
appends to its `Mailbox` inbox (inbox, queue, saved position:
`peek`/`skip`/`take`/`restart`); messages are roots. Ended pids and
`{Atom, Atom}` (until step 50) drop the message, other destinations badarg, a
refused copy fails the sender (exit 70). OTP golden `executables_send`;
`runtime_messages`.

<a id="step-46"></a>

### 46. Implement selective receive without timeout

Done 2026-10-08 (contract `docs/processes.md#receive`). `branch_clauses` lists
receive clauses like `case`. Codegen loop head peeks (`erlang_aot_receive_v1`,
`ReceiveOperation` in `abi/messages.hpp`) into a root slot; a match `take`s,
the last mismatch `skip`s; nothing left enters the wait builtin
(`erlang_aot_wait_frame_v1`). Runtime `process/receive.cpp`:
`ProcessStack::wait`; the executor parks waiting processes (`parked_`), a send
wakes them; an empty queue blocks forever (OTP); a host invocation that would
wait fails `busy`. OTP golden `executables_selective_receive`.

<a id="step-47"></a>

### 47. Implement `receive … after` timeouts

Done 2026-10-08. The after body is a receive clause without pattern
(`first_handler` = message clause count), its timeout evaluated first. The
wait answers true/false; false branches to `receive.timeout`, which `restart`s
the scan and starts the after clause (`CaseJoin::timeout`, `start_after`).
`ErrorReason::timeout_value` (25): `infinity` or 0..4294967295 checked only
when waiting; a finite deadline set at the first wait, cleared by
`take`/`restart`; executor `timers_` by deadline, sleeping until the earliest
(timer wheel: step 62B). OTP golden `executables_receive_after`.

## I. Processes and messaging (steps 48–53)

<a id="step-48"></a>

### 48. Implement links, exit signals and `trap_exit`

Done 2026-10-08 (contract `docs/processes.md#links`, `#exit-signals`).
`link/1`, `unlink/1`, `spawn_link/1,3`, `exit/2`, `exit_signal/2` (OTP 29),
`process_flag(trap_exit, Bool)`. Per-process `Signals` (link pids in order,
`trap_exit`); `scheduler/signals`: only the running process sends signals, so
targets are handled at once (end: `CallError::exited`, uncatchable; trapped:
`{'EXIT', From, R}`; drop), ended processes drained without recursion, links
signalled with `exit_reason`. Main ended by a signal: uncaught exit (normal:
exit 0). link to an ended pid: `error:noproc`, or a noproc message when
trapping. Differences: only `trap_exit` flag; no aliases for `exit/2` to a
reference. OTP golden `executables_links` (chains of 100, kill/killed,
trapped normal, self quirks past catch/after, unlink, badarg rows, main
killed/normal/linked).

<a id="step-49"></a>

### 49. Implement monitors

Done 2026-10-08 (contract `docs/processes.md#monitors`). `monitor/2`
(process, pid items; names in step 50), `demonitor/1,2` (flush/info),
`spawn_monitor/1,3`. `Signals` keeps held monitors and watchers by
`ReferenceIdentity` (now real: number, `TermFactory::reference`); ending
processes drop held monitors and send `'DOWN'` with the exit reason; ended
target: `noproc` at once; self-monitor creates nothing; flush only when the
monitor was no longer active (OTP's bif.c). Difference: other monitor types
badarg. OTP golden `executables_monitors` (reasons incl. killed/undef,
noproc, two monitors, demonitor/info/flush, self, watcher end, badarg rows).

<a id="step-50"></a>

### 50. Implement registered process names

Done 2026-10-08 (contract `docs/processes.md#registered-names`).
`register/2`, `unregister/1`, `whereis/1`, `registered/0`; sends to `Name`
(badarg when unregistered) and `{Name, nonode@nohost}` (dropped when
unregistered, other nodes dropped); `monitor(process, Name | {Name, Node})`
with `{Name, nonode@nohost}` in `'DOWN'`. Executor `names_` table, name
released before links/monitors are signalled (as OTP, probed 20,000 times).
`kvstore`, `ring` and `supervise` now compile without diagnostics. OTP golden
`executables_names` (every send form, conflicts, release, re-registration,
name monitors, badarg rows).

<a id="step-51"></a>

### 51. Collect garbage with mailboxes and suspended processes

Done 2026-10-08 (contract `docs/runtime-heap.md#waiting-and-suspended-processes`).
No runtime change was needed: messages (inbox and queue, `'EXIT'`/`'DOWN'`
too) are rewritten in place, so the list-position cursor and deadline stay
valid; delivery copies straight into the receiver's fragments (no separate
in-transit buffer); waiting/yielded processes keep frames and continuation
registers as roots and collect at the resume entry safepoint. OTP golden
`executables_mailbox_collection` (2,000-message hoarder, 40 hoarders, timeout
waiter, 3,000-deep recursion under message load; messages with shared
subterms and off-heap binaries, checked intact and in order) plus an authored
`--max-heap 65536` run of an acknowledging consumer (about 1 MB through a
64 KB cap).

<a id="step-52"></a>

### 52. Enable identity-dependent guards

Done 2026-10-08 (contract `docs/guards.md#catalog`). Immediate operations
`self`, `node`, `node_of` and `is_native_record` serve guards and bodies;
bridge builtins `node/0,1` and `is_record/1` serve funs and dynamic calls. All
81 catalog signatures implemented; the `guards` notimpl path and its
placeholder are gone (feature implemented). The guard_catalog corpus gained
114 OTP rows (node/0,1, self/0 via `is_pid(self())`, is_record/1; 16 gate
cases now accepted), services' guard-resolution rows accept `self()`; corpora
regenerated (67,748 native results). OTP golden `executables_identity_guards`
(positive `is_pid`/`is_reference`/`is_function`/`is_record/1` on real pids,
references, funs and native records, `node/1`, `self()` in a receive guard,
funs of `node/0`, `is_record/1`). `fun erlang:node/0` placeholders became
`fun erlang:apply/2`.

<a id="step-53"></a>

### 53. Decide port identity scope

Done 2026-10-08 (decision `docs/processes.md#ports`): no ports. `is_port/1`
always false; the thirteen port builtins (`open_port/2`, `port_*`,
`port_to_list/1`, `list_to_port/1`, `ports/0`) are compile-time
`[ports] notimpl` in every call and fun form (new compiler feature `ports`,
id 26, deferred, `codegen_placeholders` case); a locally defined function of
the same name wins; dynamic calls raise `undef`. Difference recorded. CLI
cases in `semantic/cases.cmake` (local, qualified, funs, shadowed) and OTP
golden `executables_ports` (is_port over real pids, references, funs;
`monitor(port, _)`/`link/1` badarg) plus an authored `undef` run.
Superseded 2026-10-08 by user direction: ports will exist (phase J2,
[57A](#step-57a)–[57F](#step-57f)).

## J. Multi-worker scheduling

<a id="step-54"></a>

### 54. Synchronize the atom table

Done 2026-10-08 (contract `docs/runtime.md#threads`). `AtomStorage` guards
both indexes with one `std::shared_mutex`: `lookup`/`boolean`/`size` share it,
`intern` looks up shared and only a new spelling takes the exclusive lock,
re-checks and publishes. Test `runtime_concurrency`: 8 threads intern 2,000
overlapping spellings from different offsets and read them back by word; all
get one word per spelling and the table grows by exactly 2,000.

<a id="step-55"></a>

### 55. Synchronize code-server publication and lookup

Done 2026-10-08 (contract `docs/runtime.md#threads`). `CodeServer` guards
`modules_` and `external_funs_` with one `std::shared_mutex`: lookups shared,
`load` and a new `external_fun` exclusive (re-check after the shared miss);
private unlocked helpers `find_fun`/`find_export`/`find_function` serve the
locked entry points. Builtins stay read-only after startup. Pins: no module is
removed before the server dies, after every context. `runtime_concurrency`:
8 threads publish 200 modules in different orders (each exactly once) while
resolving and calling them and building one external fun; after the runtime is
destroyed the threads read and release their pinned modules.

<a id="step-56"></a>

### 56. Run processes on multiple scheduler workers

Done 2026-10-08 (contract `docs/processes.md#workers`). Documented
alternative: one shared FIFO queue under one executor mutex, workers =
`RuntimeOptions::schedulers` (host default 1; programs `--schedulers N`,
1..1,024, default one per logical processor). A running process's heap/stack/
mailbox/failure belong to its worker; links, monitors, names under the lock.
A send or exit/2 to a process running elsewhere throws `builtins::Blocked`:
the typed adapter traps to a retry continuation of itself, the sender waits
among the target's blockers, then runs first and holds the target until its
own slice ends; an ended process with busy peers is finished at their slice
end. spawn_monitor monitors inside the spawn. Pid numbers and the memory
account are thread-safe. Goldens: `"workers": [1, 4]` runs each run with
`--schedulers 1` and `4` instead of the all-cores default (12 process cases; all
other cases use the default); `names`
fixture race fixed (spawn then monitor of a short-lived process), crash-report
regex order-independent; new OTP golden `executables_fairness` (three
spinners, 20 echo exchanges, kills). `runtime_processes` `workers`: four
CPU-bound processes on four workers overlap.

<a id="step-57"></a>

### 57. Handle cross-worker wakeups, timers and shutdown

Done 2026-10-08 (contract `docs/processes.md#workers`). No runtime change was
needed: every scheduling state change happens under the executor mutex, a
waiting process cannot get a message during the slice it began to wait in,
parking with a timeout wakes all idle workers, busy workers expire timers
before every slice, and `run()` joins every worker before `clear()` releases
the processes. OTP golden `executables_wakeups` (`workers` 1, 2, 4; runs:
4 rounds of arrivals against 0-2 ms timeouts, a 16-process linked spinning
chain killed by one signal with every member monitored, spawn_monitored
processes ending as their watcher wakes; `teardown` ending while processes
spin, wait and flood; `halt` from another process, exit 3). The first OTP
draft raced itself (monitor after spawn of a process ending after 0 ms):
spawn_monitor. Repeated 20 times (1,440 executions) with no failure; it is the
step-68 ThreadSanitizer workload.

## J2. Ports and port I/O

Added 2026-10-08 by user direction: ports must exist. Sockets, file I/O and
subprocesses with their stdin/stdout are ports, as are OTP's other port uses
(`fd` ports, standard I/O). This supersedes the step-53 decision (no ports);
the step-53 compile-time `[ports] notimpl` placeholders are removed as the
steps below implement each builtin.

<a id="step-57a"></a>

### 57A. Decide the port contract

Backlog: F35. Depends on: [53](#step-53), [57](#step-57).

Done 2026-10-08 (decision `docs/ports.md`). Port = immediate tag 0x7
(`TermKind2::port`), never-reused process-wide numbers, `#Port<0.N>`, order
funs < ports < pids. Executor-owned port table (driver, connected pid, links,
watchers, name, options, counters); opener linked; owner end closes the port;
close reason to links/monitors. Drivers `fd` (57B output, 57C input), `spawn`
(57D), `file` (57E, synchronous `port_control`), `tcp`/`udp` (57F, async),
internal ones via `{spawn_driver, ErlangAoTName}`. One I/O thread per runtime:
Windows IOCP (console stdin via a reader thread), POSIX `poll()` + wakeup pipe
(epoll/kqueue later: deviation from this step's draft); events delivered under
the executor mutex, deferred to the slice end of a running owner; output
queued, `port_command` never suspends. io output stays direct; stdin via an fd
port server; `file:open` returns an io-server pid. OTP probes (scratch, OTP
29.1.1): close of a linked port -> `'EXIT'` normal, `{Pid, close}` -> exit then
`closed`, `connected`, `enoent` error for a missing executable, `{packet,2}`
and `{line,L}` framing, order fun < port < pid. Prototype
`tests/prototypes/poller/` (`run.py --wsl`): Windows IOCP and WSL Linux poll()
wake an idle scheduler (9-91 us) and stop on shutdown. maint-29 re-fetched:
unchanged `21776803`.

<a id="step-57b"></a>

### 57B. Add port identities and the port table

Backlog: F07, F35. Depends on: [57A](#step-57a).

Done 2026-10-08 (contract `docs/ports.md`). `IdentityNumbers` (was
ProcessNumbers) issues pid and port numbers from separate never-reused
sequences; port words admitted in `TermAccess::identity`, printed `#Port<0.N>`,
ordered between funs and pids. Executor port table (`scheduler/ports.cpp`,
`Port`/`PortDriver` in `ports/port.hpp`, `ports/fd.cpp` output-only fd driver):
opener linked, close -> links EXIT then monitors DOWN (then `{Port, closed}`
for `{Pid, close}`), port exit-signal rules from ERTS io.c (link normal from a
non-owner dropped, exit/2 always closes, kill -> killed), badsig to the
connected process for malformed/foreign requests, `PortEvent`s (atom-only)
posted to running targets and applied in `after()`. 13 bridge builtins
(`builtins/ports.cpp`, auto-import as erl_internal), sends/link/monitor(port)/
exit/register/whereis accept ports; compiler `[ports] notimpl` removed
(feature implemented). Instead of a test-only driver the OTP golden
`executables_port_identities` (renamed from `ports`) uses `{fd,0,1}` with `out`
(never written; one open at a time to avoid OTP's driver_select reports);
runtime test `identities` adds port admission, copy, order, close. Found:
`Executor` ctor must not be noexcept (MSVC unordered_map allocates).

<a id="step-57c"></a>

### 57C. Run the I/O poller with scheduler wakeups

Backlog: F23, F35. Depends on: [57B](#step-57b).

Platform poller thread(s) (IOCP / epoll / kqueue) delivering port events as
messages to the connected process and waking it on any worker; shutdown
closes every port and stops the poller.

- Success criteria
  - [ ] No lost wakeups between port events, receives and timeouts; the
    program ends with ports open as OTP's halts.
- Tests
  - [ ] Repeated stress of port events against receive timeouts and teardown.

<a id="step-57d"></a>

### 57D. Open subprocesses as ports

Backlog: F35. Depends on: [57C](#step-57c).

`open_port({spawn, Command} | {spawn_executable, File}, Options)` with the
child's stdin/stdout (and `stderr_to_stdout`) as the port, `{packet, N}`,
`{line, L}`, `binary`, `eof`, `exit_status`, `args`, `arg0`, `env`, `cd`;
`os:cmd/1` in the project library.

- Success criteria
  - [ ] Subprocess ports match OTP on Windows, Linux and macOS within the
    recorded subset.
- Tests
  - [ ] OTP golden running owned helper programs: echo, line and packet
    framing, exit status, closing, owner death.

<a id="step-57e"></a>

### 57E. Implement file I/O and standard I/O through ports

Backlog: F26, F35. Depends on: [57C](#step-57c).

`{fd, In, Out}` ports, a file driver and a project-library `file` subset
(`open/2`, `read/2`, `write/2`, `close/1`, `read_file/1`, `write_file/2`,
`read_line/1`, `position/2`, `delete/1`, `rename/2`, `list_dir/1`) plus
`io:get_line/1,2`, `io:get_chars/2,3` and `io` writes routed through the
standard I/O port; `io:format` keeps its observable output.

- Success criteria
  - [ ] File and standard I/O results and errors (`{error, enoent}`, ...)
    match OTP for the subset.
- Tests
  - [ ] OTP golden reading/writing temporary files and reading stdin.

<a id="step-57f"></a>

### 57F. Implement sockets as ports

Backlog: F35. Depends on: [57C](#step-57c).

A TCP/UDP socket driver and project-library `gen_tcp`, `gen_udp` and `inet`
subsets over it (`connect`, `listen`, `accept`, `send`, `recv`, `close`,
`controlling_process`, active modes `true`/`false`/`once`, `{packet, N}`,
`binary`/`list`, `inet:setopts/2`, `inet:port/1`, `inet:peername/1`) on IPv4
and IPv6 loopback.

- Success criteria
  - [ ] Socket messages, errors and closing match OTP within the subset.
- Tests
  - [ ] OTP golden of an echo server and clients in one program over
    loopback (active and passive modes, packet framing, close from either
    side).

## K. End-to-end projects

<a id="step-58"></a>

### 58. Run the target fixture projects end to end

Backlog: F01, V03. Depends on: [2](#step-2), [57](#step-57). Ports (J2) are not required by the step-2 fixtures.

- Success criteria
  - [ ] Every step-2 fixture builds through its project manifest and matches
    its OTP golden at O0/O2 with 1 and N workers.
  - [ ] Remaining gaps found by fixtures are added to the backlog with owners.
- Tests
  - [ ] Fixtures run through the step-8 runner in normal CTest.

## L. Optimization and tooling

Steps 58A–58G (added 2026-10-08, user request) make type inference precise
enough that `tests/fixtures/inference/values.erl` and `base_types.erl` (every
base and built-in type of the [type language](https://www.erlang.org/doc/system/typespec.html))
reach their `expect:` signatures: today only integer constants, integer joins
and argument relations are inferred (7 of 39 and 3 of 46 functions). Every
built-in type already resolves in declarations. They depend only on the existing
inference (steps 18, 21) and may move earlier. Each step removes the `today:`
lines it closes, adds fixtures for its own cases, and keeps facts sound:
specialization (step 59) may only rely on proven facts, and every widening,
budget or unknown construct still yields `term()`.

<a id="step-58a"></a>

### 58A. Decide the inference fact domain

Backlog: F34. Depends on: [18](#step-18). **Decision.**

Publish `docs/semantic.md#inference-domain`: which facts exist (singleton
atoms and integers, `float()`, integer ranges, `number()`, `boolean()`,
tuples, lists with element and nonempty facts, maps with exact keys, funs
with arity and result, bitstrings with size), when singleton unions become a
range or a category (size thresholds), how containers widen (depth and
element budgets), and how facts relate to specs (never trusted for
representation).

- Success criteria
  - [ ] The contract names every fact kind, its join and widening rule and its
    budget; `--print-types` text for each matches the `values.erl` and
    `base_types.erl` expectations or the expectations are updated with the
    contract. Categories print by their built-in names (`boolean()`,
    `binary()`, `nonempty_binary()`, `string()`, `nonempty_string()`,
    `non_neg_integer()`, `pos_integer()`, `neg_integer()`, `number()`), bounded
    integer sets as ranges (`0..255`, `1..10`); the contract covers `pid()`,
    `port()`, `reference()`, `dynamic()` (as `term()`) and `none()`.
- Tests
  - [ ] Focused unit tests of joins and widening at the documented thresholds
    (`semantic_inference`).

<a id="step-58b"></a>

### 58B. Infer facts of all literals

Backlog: F34. Depends on: [58A](#step-58a).

Atoms, floats, characters, strings (`[97 | 98 | 99, ...]`), `[]` and
literal binaries (`<<_:16>>`) get their facts; integer literals already do.

- Success criteria
  - [ ] Every literal and every function returning one infers its fact;
    joins of mixed literals follow the 58A rules (`1 | float()`).
- Tests
  - [ ] `values.erl` literal rows (`float`, `atom`, `string`, `empty_list`,
    `binary`, `integer_or_float`) and `base_types.erl` literal rows (`nil`,
    singleton atoms, `?MODULE`, `<<>>`, `<<_:3>>`, `{}`, `#{}`, `mfa`) reach
    `expect:`; new rows for characters, negative floats and long strings at
    the widening threshold.

<a id="step-58c"></a>

### 58C. Infer operator and builtin results

Backlog: F34. Depends on: [58B](#step-58b).

Arithmetic on known integers folds within the integer limit and otherwise
yields `integer()`, `float()` or `number()` by operand facts (`/` is always
`float()`, `band 255` is `0..255`, `abs/1` of an integer `non_neg_integer()`);
comparisons, `andalso`/`orelse`/`not` and type tests yield `true`, `false` or
`boolean()`. A table gives every bridge builtin its result category: `self/0`,
`spawn/1,3` -> `pid()`, `make_ref/0` -> `reference()`, `length/1`,
`byte_size/1`, `tuple_size/1` -> `non_neg_integer()`, `float/1` ->
`float()`, `trunc/1` -> `integer()`, `list_to_atom/1` -> `atom()`,
`atom_to_list/1` -> `string()`, `integer_to_list/1` -> `nonempty_string()`,
`list_to_binary/1` -> `binary()`, `tuple_to_list/1` -> `list()`,
`list_to_tuple/1` -> `tuple()`, ... Raising paths contribute nothing to a
join, so a function that always raises infers `none()`.

- Success criteria
  - [ ] `sum() -> 3`, `product() -> 42`, `division() -> float()`,
    `comparison() -> true`, `conjunction() -> false`; folding never changes
    runtime behavior (overflow to bignums, badarith stay runtime outcomes).
- Tests
  - [ ] `values.erl` operator rows and the `base_types.erl` builtin, boolean,
    number and `no_return` rows reach `expect:`; new rows for bignum folding,
    `div`/`rem`, mixed integer/float arithmetic and the rest of the builtin
    table.

<a id="step-58d"></a>

### 58D. Infer container facts

Backlog: F34. Depends on: [58B](#step-58b).

Tuples keep element facts; lists join element facts and know nonempty or
empty; strings are lists of character facts; maps keep exact constant keys of
any kind (atoms, integers, tuples, mixed) with value facts, and an update of
an unknown map is `map()`; records keep their tuple shape. Element access
(`element/2`, patterns, `hd/1`) reads element facts back.

- Success criteria
  - [ ] Same-type and mixed-type tuples, lists and maps, nested containers and
    `{ok, 1} | {error, bad}` joins print as their `values.erl` expectations;
    width and depth past the 58A budgets widen to the category.
- Tests
  - [ ] `values.erl` container rows and the `base_types.erl` list, string,
    iolist, improper list, map update, binary construction and comprehension
    rows (`binary()`, `nonempty_binary()`, `nonempty_bitstring()`) reach
    `expect:`; new rows for records, element access, cons cells and
    containers at the budget limits.

<a id="step-58e"></a>

### 58E. Infer fun facts

Backlog: F34. Depends on: [58D](#step-58d), [35](#step-35).

`fun F/A`, `fun M:F/A` and anonymous funs (closures included) get fun facts
with their arity and the callee's or body's result fact; a call of a value
whose fact is a known fun uses that result.

- Success criteria
  - [ ] `returns_fun() -> fun(() -> 42)`, `local_fun() -> fun(() -> 5)`,
    `applies_fun() -> 6`, closures with captured facts; unknown funs and
    `apply/2,3` stay `term()`.
- Tests
  - [ ] `values.erl` fun rows and `base_types.erl` `fun_value`/`remote_fun`
    reach `expect:`; new rows for named funs, funs passed to library functions
    and funs stored in containers.

<a id="step-58f"></a>

### 58F. Infer local function inputs from their callers

Backlog: F34. Depends on: [58B](#step-58b), [18](#step-18).

A function that is neither exported nor referenced by a fun gets the join of
its call sites' argument facts as inputs, iterated with the recursive
components of step 18 (inputs widen like results after the round limit);
exported and fun-referenced functions keep `term()` inputs.

- Success criteria
  - [ ] `increment(3) -> 4` and `call_local() -> 4`; recursive local loops
    converge or widen as results do; specialization profiles stay sound.
- Tests
  - [ ] `values.erl` local rows reach `expect:`; new rows for several call
    sites, recursive locals, a local referenced by `fun f/1` and the widening
    limit.

<a id="step-58g"></a>

### 58G. Narrow facts by patterns and guards

Backlog: F34. Depends on: [58C](#step-58c), [58D](#step-58d).

Inside a clause, a matched pattern and the guard refine the facts of the
values they test: `is_integer(X), X >= 1, X =< 10` makes `X` the range
`1..10`, `{ok, V}` makes the matched value a two-tuple, `is_float` /
`is_integer` split number joins. Refinements apply only within the clause
(and the guarded branch of `case`/`if`/`receive`).

- Success criteria
  - [ ] `bounded(term()) -> 1..10`, `scaled(term()) -> number()`; a refined
    fact never escapes the clause that proved it.
- Tests
  - [ ] `values.erl` and `base_types.erl` reach `expect:` for every row (no
    `today:` lines left: `1..10`, `0..255`, `pos_integer()`,
    `neg_integer()`, `infinity | non_neg_integer()`); new rows for range
    guards, tuple and list patterns, map patterns and refinements that must
    not leak.

<a id="step-58h"></a>

### 58H. Reject specifications that contradict inferred types

Backlog: F34. Depends on: [58A](#step-58a), [58G](#step-58g). Added
2026-10-08 (user request).

A `-spec` must not contradict what inference proves: a function's inferred
result must be a subtype of (equal to or narrower than) the union of its
overloads' declared results, and a call whose inferred arguments fit no
overload's declared arguments contradicts the callee's spec. Today
`types/contracts.cpp` only warns, and only for known integer singletons.
Replace it with a subtype relation over the whole 58A fact domain
(`semantic::types::subtype(declared_graph, declared, inferred_graph,
inferred)`) and make a contradiction a compile error at the `-spec`.

- Rules: unknown facts (`term()`, widened or over budget) never contradict;
  `dynamic()`, `any()` and `term()` admit everything; `none()` /
  `no_return()` admits only a function that never returns, and a function
  that never returns fits any result; type variables and `when` constraints
  are checked through their bounds (an unconstrained variable admits
  everything); opaque and nominal types compare by their own identity outside
  their module and by definition inside it; remote types resolve through the
  batch (unresolved remote types admit everything); `-callback` specs are
  not checked against implementations (behaviours, step 73).
- OTP's compiler does not check specs (Dialyzer does, as warnings): record
  the error in `docs/differences.md`; keep the check sound (no false error is
  acceptable) and report the declared and inferred types in the diagnostic.

- Success criteria
  - [ ] Every inferred result and every call with known arguments that
    contradicts a spec is a compile error naming the function, the declared
    type and the inferred type; a narrower inferred type, an unknown fact and
    every construct in the rules above compile without one.
- Tests
  - [ ] Fixtures (`tests/fixtures/inference/contracts/`) with one
    contradiction per fact kind of 58A (literal, operator result, container,
    fun, range, union, overloads, constraints, opaque/nominal, remote type,
    `no_return()`) each fail with the expected diagnostic; `values.erl`,
    `base_types.erl` and every existing program and fixture compile without
    one (their specs hold); `codegen_types`' deliberate `value() -> 42` vs
    `atom()` case becomes an error test.

<a id="step-59"></a>

### 59. Make specialization remove real source checks

Backlog: F29. Depends on: [58](#step-58), [58G](#step-58g).

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

<a id="step-62b"></a>

### 62B. Keep receive timers in a timer wheel

Backlog: F23, F25. Depends on: [47](#step-47), [57](#step-57). Added
2026-10-08 during step 47 (user request).

Step 47 keeps one ordered map of deadlines and reads the monotonic clock
before every time slice to find expired receive timeouts. Replace it with a
runtime timer wheel (hashed, hierarchical slots of millisecond ticks, as ERTS
`erl_time_sup`/`erl_hl_timer` do): arming and cancelling a timer is constant
time, the scheduler advances the wheel from a coarse clock reading taken at
most once per tick (not per slice), and only the slots whose time has come
are consulted when a scheduled timer must fire; waiting processes are never
scanned for deadlines.

- Success criteria
  - [ ] Arming, cancelling (a message arrives first) and firing timers cost
    constant expected time per timer; the clock is read at most once per tick
    while processes run, and an idle scheduler sleeps exactly until the next
    occupied slot.
  - [ ] Timeouts never fire early and fire within one tick of their deadline;
    `after 0`, `infinity` and the 0..4294967295 range keep their step-47
    behavior; with multiple workers (phase J) each worker's wheel, or a shared
    one under its synchronization, keeps these guarantees.
- Tests
  - [ ] Existing goldens (`executables_receive_after`,
    `executables_selective_receive`) pass unchanged; a focused runtime test arms
    many timers, cancels most, and checks firing order and that cancelled ones
    never fire; clock readings per slice recorded descriptively, not gated.

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
