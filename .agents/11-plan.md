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
| O. Final closure | [78](#step-78) | all |

---

## A. Baseline and fixtures

<a id="step-1"></a>

### 1. Refresh the OTP reference and record a fresh baseline

Backlog: V03. Depends on: completed baseline.

Check upstream `maint-29` per the reference procedure and capture the current
gate as the starting point for this plan.

- Success criteria
  - [x] Pin, checkout, corpus hashes and grammar evidence agree with upstream,
    or the unchanged revision is recorded with the fetch date.
  - [x] The full gate passes and its test/quality-unit counts are recorded in
    `docs/validation.md`.
- Tests
  - [x] Full gate on a fresh build.
  - [x] Opt-in grammar, corpus and source audits pass against the pin; any
    drift is reported, not hidden by regeneration.

### 1A. Tests run time too long

The test running is taking too much time, can we split tests into fast and full?
- Use fast during development
- Use full test mode once per major feature completion.
- Success criteria: Test runs while development is ongoing take 1-2 min instead of 700 seconds now.

Done 2026-10-03. `ERLANG_AOT_TEST_MODE=fast` (presets `debug-fast`/`windows-debug-fast`,
`make test`, `make-test.bat`) runs 122 tests in parallel in about 60 s: golden corpora use
O0 positional plus O2-off project (`tests/compiler/patternmatch/matrix.py`), mutations run
once, and `full_only` tests are excluded. Full mode (default when unset) passes 125/125 in
85 s with `-j 16`, versus 729 s serially.

### 1B. Quality check checks too much

Quality checking should only check the files changed in the working tree, no need to hit same clean files repeatedly, unless they depend on changed files.
- Success criteria: Scope of static checks and formatting is reduced for each change, instead of doing entire source every time.

Done 2026-10-03. `check-quality`, `make format` and `make-format.bat` default to files changed
since `HEAD` (plus untracked); `check-quality-all`, `make format-all` and `FORMAT_SCOPE=all`
keep full scans. `cmake/quality_scope.py` selects Lizard files and clang-tidy units, adding
units that include a changed header via Ninja deps; `.clang-tidy`, `cmake/` or production
CMake changes (or missing deps) select every unit. Full formatting fixed two pre-existing
unformatted declarations; the mutation hang guard rose from 5 s to 30 s after a load-only
timeout under parallel fast CTest (the case takes 16 ms alone).

### 1C. Run the available documentation check

The documentation in /doc/ was never createad intentionally rather as byproduct of implementation passes. Clean it up and drop duplicated, bloated text, keep only brief informational pieces which may later help build good documentation.

Done 2026-10-03. 81 files (5,370 lines plus about 500 KB of evidence JSON) became 15 brief
notes indexed by `docs/README.md`: frontend (`preprocessor`, `parser`, `projects`), compiler
(`compile`, `semantic`, `specialization`, `abi`, `features`), language (`patterns`, `guards`,
`terms`), `runtime`, `otp-reference` and `validation`. Per-step validation records, evidence
JSON and step logs were removed; [validation history](../docs/validation.md#history) keeps
dates, revisions and counts, and originals remain in Git at `2777c98`. Stale claims (configure
requiring OTP, revision-2 descriptors, placeholder compilation) were corrected; every
repository link was repointed and checked.

<a id="step-2"></a>

### 2. Author target program fixtures and their feature map

Backlog: V03, F26. Depends on: [1](#step-1).

Write 4–6 small project-owned Erlang programs that represent the end goal (for
example: text/number processing CLI, binary protocol parser, recursive data
structure library, process ring, key-value server loop, supervisor-style
restart). Record which language features and builtins each needs.

- Success criteria
  - [x] Each fixture is original code with a project manifest, entrypoint,
    expected stdout and exit status generated once from OTP.
  - [x] A table maps every fixture to the plan steps it depends on.
- Tests
  - [x] Explicit regeneration reproduces each golden under OTP.
  - [x] A normal CTest compiles each fixture and checks today's expected
    unsupported-feature diagnostics (updated as later steps land).

Done 2026-10-03. Six fixtures in `tests/fixtures/programs/` (`textstats`, `frames`, `avltree`,
`ring`, `kvstore`, `supervise`) with the feature map in its README. Goldens come from OTP 29.1.1
through `tests/compiler/programs/oracle.escript` under the proposed step-3 contract;
`regenerate.py --check` reproduced all six three times. CTest `programs_compile` verifies hashes
and exact `compile.txt` diagnostics; opt-in `programs_oracle` reruns OTP. The fixtures showed
`++`/`--` had no owner, so step 37 now lists them.

## B. Production executables

<a id="step-3"></a>

### 3. Decide the entrypoint, arguments and exit-status contract

Backlog: F01. Depends on: [1](#step-1). **Decision.**

Define how an executable chooses its entry function, receives arguments and
reports its result. Proposed default: `main/1` receives argv as a list of
strings; normal return exits 0; `erlang:halt/0,1` sets the status; an uncaught
exception prints a report to stderr and exits 1. Choose CLI (`--entry M:F`) and
project-manifest spelling, including whether the manifest key extends schema 1.

- Success criteria
  - [x] `docs/executables.md` defines entry selection, argv encoding, exit
    codes, stdout/stderr use and failures for missing or unexported entries.
  - [x] The compiler validates the entry selection (exists, exported, arity)
    and reports located errors, without linking yet.
- Tests
  - [x] CLI cases for valid entry, missing module, missing function, wrong
    arity and unexported entry, checking exit status and diagnostics.
  - [x] Project manifest cases for the new entry key, including unknown/invalid
    values.

Done 2026-10-03. Chosen: `--entry MODULE[:FUNCTION]` and optional schema-1 target key
`entry`; function default `main`, arity always 1 (argv strings). Without a selection, `-o`
uses the only module exporting `main/1`. Exit 0 on return/`halt()`, `halt(N)` sets N, any
escaping exception (including `exit(normal)`) or killing signal exits 1, runtime failure 70.
`driver/entry.cpp` resolves after indexing so entry and capability errors report together;
CTest `linking_entry` covers CLI, manifest and detection cases. Fresh gate: 124 fast tests,
260 quality units.

### 3A. Add escript compile mode

Consider a new option (or better auto detect situations) when user wants to 
compile a escript file/compile file into a escript executable. Those have slightly
different start routine name and arguments. Consult with Erlang/OTP documentation
how escript file main functions are to be defined.

Done 2026-10-03. Auto-detected, no option: a first line starting with `#!` selects escript rules
from OTP's `escript` docs and `escript.erl` (`docs/executables.md#escripts`). `driver/escript`
rewrites the header (shebang replaced by `-module('<file>__escript').` when the first form is
not `-module`, line numbers kept; `%%!` warns), `semantic/escript` requires and exports `main/1`
and validates `-mode`, and entry detection prefers the batch's only escript, whose uncaught
exceptions will exit 127. CTest `linking_escript`; acceptance matched OTP 29 `escript` once. Fresh gate:
125 fast tests, 262 quality units.

<a id="step-4"></a>

### 4. Add runtime term printing and `erlang:display/1`

Backlog: F01, F26. Depends on: [1](#step-1).

Implement `~w`-style text output for every admitted term in the runtime, used
for program output, uncaught-error reports and later `io` support.

- Success criteria
  - [x] Atoms (with quoting), integers, floats (OTP shortest round-trip form),
    tuples, lists, improper lists, maps (OTP key order), bitstrings and records
    print exactly as OTP `~w`.
  - [x] `erlang:display/1` is callable from source and writes one line.
- Tests
  - [x] Golden comparison of printed output against OTP for the existing owned
    corpora values (reuse their expected results as inputs).
  - [x] Deep and wide terms print within bounded work; output write failure is
    reported.

Done 2026-10-03. `format_term` (`runtime/src/terms/term_text*.cpp`) renders `~w` and the
emulator's `erlang:display/1` text iteratively under a 64 MiB cap; `RuntimeOptions::standard_output`
(default stdout) receives display lines. `erlang:display/1` is a body-only service lowered to
`erlang_aot_display_v1` (new status `output_failure`). Maps print in map-key order (OTP `~kw`):
OTP's default order follows atom-table indices (varying between VM runs) or hashing, so it is not
reproduced and display goldens skip such values. `tests/fixtures/printing/` holds 9,542 values
(corpus results plus authored edge cases; `regenerate.py --check` reproduced 3x); CTests
`runtime_printing` (goldens, 100k-deep, 1M-wide, shared-subterm cap, sink failure) and
`printing_display` (154 compiled display calls against real OTP stdout, all policies).

<a id="step-5"></a>
  
### 5. Generate the startup object

Backlog: F01. Depends on: [3](#step-3), [4](#step-4).

Emit a native `main` that creates the runtime, registers every module of the
batch, builds argv, calls the entry and maps the outcome to the exit status.

- Success criteria
  - [x] Startup registers modules transactionally and shuts the runtime down in
    order on every exit path.
  - [x] Uncaught errors print the step-3 report; ABI mismatch fails before
    entry.
- Tests
  - [x] Link startup plus modules manually with the existing harness recipe and
    check normal return, `halt/1`, uncaught error and argv passing.
  - [x] IR inspection of the startup object at O0/O2 on both word widths.

Done 2026-10-04. An explicit entry adds a startup module (`codegen/startup`, artifact `eav1_start`)
whose `main` passes an `abi::v1::StartupDescriptor` to the runtime's `erlang_aot_main_v1`
(`runtime/src/startup/`): all descriptors are ABI-checked before the first registration, a failed
registration discards the runtime before entry (exit 70), argv is decoded per platform (CRT wide args
on Windows), reports follow a stdout flush and teardown is ordered on every path. `erlang:halt/0,1`
became a body builtin (`erlang_aot_halt_v1`, `CallError::halted`) so the exit paths are testable.
CTests `linking_startup` (O0 positional, O2 project and escript objects linked by CMake without a
harness; argv, return, halt, slogan, badarg, badmatch, function_clause, escript 127; startup IR for
x64/x86 MSVC, x64 Linux, ARMv7 at O0/O2) and `runtime_startup` (ABI/flag mismatch, duplicate module,
missing entry never run the entry).

<a id="step-6"></a>

### 6. Link executables from positional CLI inputs

Backlog: F01. Depends on: [5](#step-5).

Drive Clang to link module objects, startup and the matching runtime library;
replace the explicit executable-output "not implemented" failure.

- Success criteria
  - [x] `erlangaot -o app[.exe] a.erl b.erl` produces a runnable program.
  - [x] Missing runtime, missing linker, link errors and target mismatch fail
    with clear diagnostics and publish no partial executable.
- Tests
  - [x] Build and run the two-module example at O0/O2; compare stdout and exit
    status.
  - [x] Failure cases: absent runtime library, wrong target triple, unwritable
    output path, existing output preserved on failure.

Done 2026-10-04. New `compiler/src/linking/` (`erlang_linking`, LLVM-private): positional `-o` keeps
objects in memory, stages them in a private `.erlangaot-link-*` directory beside the output and runs
`<clang> --driver-mode=g++ --target=<triple>` with the runtime archive, then replaces the output (`.exe`
added for Windows targets without an extension). Options `--linker` (else `clang++`/`clang` on PATH, then
`%ProgramFiles%/LLVM/bin`) and `--runtime-library` (else the build's archive, recorded relative to
`erlangaot`); LLVM Object checks every member's arch/format against the target. Linking works outside a
VS developer shell. `examples/compile/client.erl` gained `main/1`. CTest `linking_executable` (example
O0/O2, argv/halt, escript 127; absent/non-archive/wrong-target runtime, absent linker, real undefined-symbol
link error, file-as-directory, directory and input destinations, existing output preserved, no staging
left); entry/escript/CLI tests now stop at an absent runtime library; project `-o` stays notimpl (step 7).
Entry-detection hints spell `--entry MODULE[:FUNCTION]` and, for project targets, the manifest
`entry = "MODULE[:FUNCTION]"` key. Fresh gate: 131 fast tests, 272 quality units.

### 6A. Link a single project target with explicit `-o`

Users combine `--project` with `-o` and expect a program. Project `-o` already requires exactly
one selected target, so that target links like a positional batch; manifest `output` without `-o`
and multi-target linking stay in step 7.

- Success criteria
  - [x] `erlangaot --project P [--target T] [--entry M] -o PATH` links and runs.
  - [x] No path reports `[executable linking] notimpl`; the catalog entry is implemented.
- Tests
  - [x] `linking_executable` links project targets (manifest and CLI entry, O0/O2) and reports
    a project-prefixed failure; entry and placeholder tests no longer expect notimpl.

<a id="step-7"></a>

### 7. Link executables from project targets

Backlog: F01. Depends on: [6](#step-6).

Use the reserved manifest `output` paths so each selected target produces its
executable.

- Success criteria
  - [x] Each selected target links to its output (default `build/<target>[.exe]`);
    all targets validate before any publication.
  - [x] A later target failure leaves earlier valid outputs unchanged.
- Tests
  - [x] Multi-target project workflow with selection, CLI `-o` rules and output
    aliasing checks.
  - [x] Run each produced executable and compare its output.

Done 2026-10-04. Without `--emit` or a check/print action, a project build links every selected
target that requests an executable (manifest `output` or `entry`, CLI `-o` or `--entry`) to CLI `-o`,
else `output`, else `<manifest-dir>/build/<target>`; other targets stay in-memory library builds (the
existing compile-check use of library targets keeps working, and `--new-project` templates already
carry `output`). `linking::stage_executable` links into a private staging directory (missing
manifest-output directories created), the driver queues `PendingExecutable`s and
`publish_executable` replaces outputs only after every target succeeded; names equal only after
`.exe` are rejected before publication. `--linker`/`--runtime-library` also apply to linking project
builds. CTest `linking_project` (fixture `tests/fixtures/linking/project/`: all targets at O0/O2 run
and compared, selection, `-o`/`--entry` rules, runtime-option failures, later link failure keeping an
existing output, plan and `.exe` aliasing); `project_workflow`/`codegen_project` now expect
`no entry point` for output-bearing targets without `main/1`. Fresh gate: 132 fast tests,
check-quality (changed scope: 13 Lizard files, 33 tidy units).

<a id="step-8"></a>

### 8. Add the executable golden test runner

Backlog: F01, V04. Depends on: [7](#step-7).

One shared CTest helper: compile Erlang sources to an executable, run it with
arguments, and compare stdout, stderr pattern and exit status with an owned
golden. Later steps use it for end-to-end tests.

- Success criteria
  - [x] Adding a case needs only source files and a golden file.
  - [x] The runner covers the four policy combinations (O0/O2 ×
    specialization on/off) without duplicating code per test.
- Tests
  - [x] Port the documented two-module demo to the runner.
  - [x] Self-check: a deliberately wrong golden fails with a readable diff.

Done 2026-10-04. A case is a directory under `tests/fixtures/executables/` with Erlang sources and
`golden.json`: authored `entry`, `runs[].args`, optional `runs[].stderr` regex (required when OTP
writes stderr) and `sources`, plus OTP `exit_status`/`stdout`, oracle version, pin and source hashes
written by `tests/compiler/executables/regenerate.py` (programs `oracle.escript`; `--check` reproduced
both cases). CMake globs cases into `executables_<case>`; `run.py` rejects stale or ungenerated goldens,
links under `matrix.py` (full: O0/O2 × specialization on/off × positional `--entry -o` and project
manifest `entry`/`output`; fast: O0 positional, O2-off project) and prints unified stdout diffs plus
exit/stderr mismatches. Cases `demo` (the `examples/compile` sources) and `exits` (argv, `halt(3)`,
`badmatch`, `function_clause` across modules); `executables_selfcheck` checks the diff for a wrong
golden and the early stale-golden failure. `linking_executable` keeps its `-Os` size and replacement
checks. Step 58 adapts the program fixtures' layout to this runner. Phase B closes: fresh fast
gate 135 tests; full `-j 16` 138/138 in 235 s; `check-quality-all` passes 272 units.

## C. Classic process heap

Review of the runtime heap at phase B close (`runtime/src/memory/`,
`term_layout.hpp`, `roots.hpp`) found a design that cannot evolve into BEAM-style
collection:

- `HeapStorage` is a list of chunks that never move, so cells cannot be
  compacted or copied and retained capacity only grows.
- Each published cell is a node in a per-process `std::map` index
  (`HeapStorage::objects`); admission and access depend on it, so heap words
  alone are not parseable.
- Every bitstring cell has a fixed 64-byte inline array and an embedded
  `shared_ptr`, and C++ members are released through a destructor registry
  indexed by address; nothing can move such a cell or find its dead copies.
- Host `Term` handles hold `shared_ptr<HeapStorage>` and raw `HeapObject`
  pointers that no collector could rewrite.
- Generated root frames are separate heap-allocated buffers rather than one
  process stack, and there is no overflow area for allocation that must not
  move the heap.

Target design, following classic ERTS:

- A heap is a flat array of words. A boxed object starts with a header word
  (primary tag `00`, kind, count of following words); a cons cell is two term
  words without a header. Untraced payload (bignum limbs, float bytes, binary
  data) is counted in its header, so a walker parses any heap area left to
  right and the collector knows every value by its header.
- Each process has one heap (BEAM `heap`) where all values allocate. A
  generational old heap with minor collections (BEAM `old_heap`) is deferred;
  the layout must keep it addable later.
- Each process has a stack of root frames, kept separate from the heap (BEAM
  grows it down from the heap end; a separate stable buffer is acceptable).
- When a value must be allocated but the heap cannot move or grow (critical
  section, no safe point), it goes into a heap fragment; fragments form a
  chain owned by the process and are merged into the heap by the next
  collection.
- Binaries larger than 64 bytes float outside every process heap as immutable
  shared buffers. A boxed off-heap binary cell (BEAM ProcBin) holds a
  `std::shared_ptr` to its buffer and is linked into a per-process off-heap
  list. Moving the cell move-constructs that member; the list sweep after
  collection and teardown destroys it in dead cells.

Steps 8A–8I replace the storage behind the existing services without changing
generated-code ABI or observable program behavior; every golden must pass
unchanged after each step. Generated code keeps raw words between runtime
calls until step 26, so until then a heap moves only at explicit host-requested
safe points and generated-code allocation overflows into fragments.

<a id="step-8a"></a>

### 8A. Decide the classic process heap contract

Backlog: F03, F04. Depends on: [8](#step-8). **Decision.**

Publish `docs/runtime-heap.md` with the review above and its replacement.

- Success criteria
  - [x] Word-level layout: header encoding (tag, kind, word count), cons cells,
    untraced payload, filler words for padding and rolled-back tails, and a
    cell table for every admitted kind on 32- and 64-bit words.
  - [x] Areas and sizing: heap, stack, fragment chain and off-heap list; minimum heap size, growth sequence and one budget
    (`limit_bytes`) across all areas, including off-heap binary bytes.
  - [x] Admission rule that replaces the object index while still rejecting
    forged, interior, stale and foreign words.
  - [x] Safe points: when the heap may move, when allocation must use a
    fragment, and how host handles, stack frames and owned error payloads are
    found and rewritten.
  - [x] Scope changes for steps 23–28 and 45 are recorded in this plan.
- Tests
  - [x] Baseline measurement of an allocation-heavy runtime kernel and the
    per-context footprint, recorded in the document for 8I (not gated).

Done 2026-10-04. `docs/runtime-heap.md` fixes the contract: header word (tag `00`, five kind bits,
word count from bit 7), headerless cons, zero-word and `filler` padding, a per-kind cell table (map
header now counts words, 2 per entry), one heap plus fragments, a segmented stack, no old heap,
ERTS sizing from 233 words, one `limit_bytes` budget including created off-heap buffers, start-bitmap
admission and host-only safe points until step 26. Per user direction, binaries over 64 bytes stay
`std::shared_ptr` buffers outside every heap: the `refc_binary` cell holds the pointer, joins a
per-process off-heap list and is moved by move-constructing that member (8B updated). Full-only
CTest `runtime_heap_measurements` recorded the baseline at `bb09359`: 100k-cell kernel 264/81 ms,
700,000 used words, 24 MB index side bytes (about 80 per cell), 66 KB and 8,192 heap words per
context.

<a id="step-8b"></a>

### 8B. Split binary cells and add the off-heap list

Backlog: F03, F09. Depends on: [8A](#step-8a).

Replace the fixed `BitCell` with two cells: a variable-size heap binary (bit
length and data words, at most 64 bytes) and an off-heap binary (bit offset,
bit length, `std::shared_ptr` to the shared buffer, off-heap link). The process
keeps an off-heap list of its off-heap binary cells instead of the
`HeapDestructor` registry.

- Success criteria
  - [ ] Every layout in `term_layout.hpp` is trivially copyable except the
    off-heap binary, whose only C++ member is the `shared_ptr` (`static_assert`
    on its two-word size); one relocation hook moves that member.
    `HeapDestructor` and `HeapStorage::resources` are gone.
  - [ ] Shared buffers are released exactly once when the last cell holding
    them dies (today: owning heap teardown); extracted tails keep their buffer
    alive.
- Tests
  - [ ] Bitstring, printing and executable goldens pass unchanged.
  - [ ] Focused runtime test: buffer counts across tails, rollback after a
    failed construction, and context teardown.

<a id="step-8c"></a>

### 8C. Make heap areas parseable and add a heap walker

Backlog: F03, F04. Depends on: [8B](#step-8b).

Works on the existing chunks first, so the walker is proven before storage
changes. Prefer C++ style design.

- Success criteria
  - [ ] Every allocated word belongs to a boxed object with a valid header, a
    cons pair or filler; untraced payload is never reported as a term.
  - [ ] `walk(area, visitor)` visits objects in address order and yields their
    term slots; a debug verifier checks that every boxed/list slot points to an
    object start owned by the same process.
- Tests
  - [ ] Focused runtime test walks heaps built from every admitted layout,
    nested and shared; 32-bit cell sizes are checked by `static_assert`.
  - [ ] Allocation failure injected mid-construction leaves a heap the verifier
    accepts.

<a id="step-8d"></a>

### 8D. Admit heap words by header instead of the object index

Backlog: F03. Depends on: [8C](#step-8c).

Replace `HeapStorage::objects` with a range check against the process's areas,
a per-area object-start bitmap (one bit per word, set on publish and cleared on
rollback) and header/tag agreement. Accessors decode kind, count and payload
from the header.

- Success criteria
  - [ ] Forged, interior, stale and foreign words are rejected before any load
    outside the checked range.
  - [ ] Publication allocates nothing per object; rollback restores the bitmap.
- Tests
  - [ ] Existing ownership and forged-word runtime tests and all goldens pass
    unchanged.
  - [ ] Focused test with interior pointers into tuples, maps, bignums and
    binary payload.

<a id="step-8e"></a>

### 8E. Make host `Term` handles relocatable roots

Backlog: F02, F03. Depends on: [8D](#step-8d).

A host `Term` for a heap value refers to a slot in a per-process handle table
(stable nodes with a free list) that a collector can rewrite; liveness still
comes from the context lifetime token. `GeneratedRoots` handoffs use the same
slots.

- Success criteria
  - [ ] No `Term` stores a raw cell pointer; enumerating the table yields every
    host-held root.
  - [ ] Expired-context results and handle destruction after teardown are
    unchanged.
- Tests
  - [ ] Existing host-handle and lifetime tests and all goldens pass unchanged.
  - [ ] Focused test: many handles copied, moved and destroyed; a reused slot
    never revives an expired handle.

<a id="step-8f"></a>

### 8F. Move generated root frames onto a process stack

Backlog: F02. Depends on: [8E](#step-8e).

Replace per-frame buffers with one per-process stack, separate from the heap,
holding frames as BEAM-style Y-register windows. `erlang_aot_roots_enter_v4` and
`erlang_aot_roots_leave_v4` keep their ABI.

- Success criteria
  - [ ] A live frame's address stays stable (reserved capacity or stable
    segments, as chosen in 8A); the 1,000,000-word and 4,096-frame limits and
    LIFO errors are unchanged.
  - [ ] Stack enumeration yields every generated root slot.
- Tests
  - [ ] Existing root, startup and executable goldens pass unchanged.
  - [ ] Focused tests for limit exhaustion, LIFO violation and restore after a
    failed call.

<a id="step-8g"></a>

### 8G. Replace chunks with a contiguous heap and heap fragments

Backlog: F03. Depends on: [8F](#step-8f).

The heap is one block `[start, top, end)` sized from a minimum heap size,
with bump allocation. A request that does not fit while the heap may not move
allocates a heap fragment sized to fit and chained to the process. Reservation
rollback resets `top` or drops the newest fragment. The alignment parameter is
removed (word alignment only).

- Success criteria
  - [ ] All allocation goes to the heap or a fragment; admission and the
    walker cover both.
  - [ ] Heap plus fragments stay within `limit_bytes`; `limit_exceeded`,
    `out_of_memory` and the one-reservation rule are unchanged.
- Tests
  - [ ] All goldens pass unchanged.
  - [ ] Focused tests: overflow into fragments, rollback across a fragment
    boundary, exhaustion with fragments, and many contexts with the default
    small heap.

<a id="step-8h"></a>

### 8H. Collect on explicit host request with a copying collector

Backlog: F03, F04. Depends on: [8G](#step-8g). Absorbs the former step 25.

Full-sweep Cheney copy: roots are handle slots, stack frames and owned error
payloads; live objects from the heap and fragments move into a new block
sized by the growth policy, leaving forwarding headers; roots are rewritten;
the off-heap list is swept and fragments are freed. Allowed only at a safe
point: until step 26, when the context is not running generated code.

- Success criteria
  - [ ] Rooted values stay valid and equal, internal sharing is preserved,
    unreachable cells are reclaimed and last-owner binaries are released once;
    the heap grows or shrinks per policy.
  - [ ] A request at an unsafe point returns `unsafe_point` with no change;
    failure to allocate the new block leaves the heap untouched.
  - [ ] `ProcessHeap::collect` leaves the deferred-services table; statistics
    report heap, fragment, stack and off-heap sizes.
- Tests
  - [ ] Focused runtime tests: host handles across repeated collections, nested
    and shared graphs of every layout, binary release counts, verifier after
    each collection.
  - [ ] Injected failure of the new-block allocation.

<a id="step-8i"></a>

### 8I. Close the heap rework

Backlog: F03, F04. Depends on: [8H](#step-8h).

- Success criteria
  - [ ] `docs/runtime.md`, `docs/terms.md`, the ABI root-scope notes,
    `arch.md` and `files.md` describe the new heap; no reference to the chunk
    list, object index or cell destructors remains.
  - [ ] The 8A kernel and footprint are measured again and compared in
    `docs/runtime-heap.md` (descriptive, not gated).
- Tests
  - [ ] Fresh full-mode CTest and `check-quality-all` pass; counts recorded in
    `docs/validation.md`.

## D. Control flow and exceptions

<a id="step-9"></a>

### 9. Lower `begin`/`end` blocks and `case` expressions

Backlog: F13, F14, F16. Depends on: [8I](#step-8i).

Reuse the clause/guard matcher for `case` clauses; implement branch-variable
export (bound in every clause) and unsafe-variable diagnostics.

- Success criteria
  - [ ] Clause order, guards, nested `case`, exported and unsafe variables
    match OTP; no matching clause raises `{case_clause, Value}`.
  - [ ] Inference joins branch facts conservatively.
- Tests
  - [ ] Golden programs for selection, fallthrough, nested cases, exported
    variables and `case_clause`.
  - [ ] CLI diagnostics for unsafe and unbound variables matching OTP lint
    wording/classes.

<a id="step-10"></a>

### 10. Lower `if` expressions

Backlog: F14, F16. Depends on: [9](#step-9).

- Success criteria
  - [ ] Guard-only clauses select in order; no true guard raises `if_clause`;
    variable export follows step 9 rules.
- Tests
  - [ ] Golden programs for guard sequences, failing guards, `true` fallback and
    `if_clause`.

<a id="step-11"></a>

### 11. Raise exceptions from source

Backlog: F20. Depends on: [9](#step-9).

Implement `erlang:error/1,2,3`, `throw/1` and `exit/1` through the checked error
channel with class and owned reason.

- Success criteria
  - [ ] All three classes propagate through local/remote calls unchanged.
  - [ ] Uncaught exceptions reach startup and print the step-3 report.
- Tests
  - [ ] Golden programs raising each class at different call depths; stderr
    report and exit status checked.

<a id="step-12"></a>

### 12. Lower `catch Expr`

Backlog: F20. Depends on: [11](#step-11).

- Success criteria
  - [ ] Values follow OTP: thrown value, `{'EXIT', Reason}` for exit, and
    `{'EXIT', {Reason, Stack}}` for error (stack per step 15; placeholder
    documented until then).
  - [ ] Bindings inside `catch` follow OTP safety rules.
- Tests
  - [ ] Golden programs for each class, nested catches and catch of runtime
    errors (`badmatch`, `function_clause`, `badarith`).

<a id="step-13"></a>

### 13. Lower `try … of … catch`

Backlog: F13, F20. Depends on: [11](#step-11).

- Success criteria
  - [ ] Class/reason patterns and guards select handlers in order; unmatched
    exceptions re-raise unchanged; `of` clauses fail with `try_clause`.
  - [ ] Exceptions inside `of` clauses and handlers are not caught by the same
    `try`.
- Tests
  - [ ] Golden programs for each class, default `throw` class, nested tries,
    re-raise and `try_clause`.

<a id="step-14"></a>

### 14. Lower `try … after`

Backlog: F20. Depends on: [13](#step-13).

- Success criteria
  - [ ] `after` runs exactly once on normal return, caught and uncaught
    exceptions; its value is discarded; an exception inside `after` replaces
    the original.
  - [ ] Rooted temporaries are released on every path.
- Tests
  - [ ] Golden programs observing `after` execution order with
    `erlang:display/1` on each path.
  - [ ] Allocation fault injected inside `after` keeps cleanup and ownership
    correct.

<a id="step-15"></a>

### 15. Provide stack traces and `erlang:raise/3`

Backlog: F20. Depends on: [13](#step-13).

Choose the stack-trace content (at least `{M, F, Arity, []}` frames; source
locations optional) and bind it in `Class:Reason:Stack`.

- Success criteria
  - [ ] Stack terms are well-formed and bounded in depth; documented
    differences from OTP are explicit.
  - [ ] `erlang:raise/3` re-raises with a supplied stack;
    `erlang:get_stacktrace` stays rejected as in OTP 29.
- Tests
  - [ ] Golden programs comparing the top frames' module/function/arity with
    OTP.
  - [ ] Malformed stack argument to `raise/3` behaves as OTP.

<a id="step-16"></a>

### 16. Lower `maybe` expressions

Backlog: F13, F16. Depends on: [9](#step-9).

- Success criteria
  - [ ] `?=` short-circuits on mismatch, `else` clauses select on the value,
    and no matching `else` raises `{else_clause, Value}`.
  - [ ] Feature enablement follows the preprocessor feature settings.
- Tests
  - [ ] Golden programs for success, early exit, `else` selection and
    `else_clause`; feature-disabled source is rejected.

## E. Execution model, recursion, comprehensions

<a id="step-17"></a>

### 17. Decide the frame and continuation model

Backlog: F02, F21, F22. Depends on: [11](#step-11). **Decision.**

Compare explicit heap frames, LLVM coroutines and CPS-style lowering with a
small compiled prototype that calls, recurses, yields and resumes.

- Success criteria
  - [ ] `docs/execution-model.md` defines call, return, tail call, yield,
    resume, exit, exception propagation and root visibility for each frame.
  - [ ] The choice works on all required targets (including Windows x86 and
    32-bit ARM) or names the fallback per target.
- Tests
  - [ ] The prototype runs return, deep recursion, yield/resume and error exit
    on the host, recorded in the decision document.
  - [ ] IR inspection on both word widths for the chosen lowering.

<a id="step-18"></a>

### 18. Accept recursive call graphs in analysis

Backlog: F21. Depends on: [17](#step-17).

Remove the acyclic-batch restriction: resolve recursive and mutually recursive
components and run bounded inference over them.

- Success criteria
  - [ ] Self, mutual and cross-module recursion compile; inference terminates
    with widening and stays sound.
  - [ ] Cycle-rejection diagnostics are removed only where lowering supports the
    case.
- Tests
  - [ ] `--print-types` goldens for recursive functions.
  - [ ] Golden programs for factorial, mutual even/odd and cross-module
    recursion with small depths.

<a id="step-19"></a>

### 19. Implement proper tail calls

Backlog: F21. Depends on: [18](#step-18).

- Success criteria
  - [ ] Local, mutual and remote tail calls run in constant native stack.
- Tests
  - [ ] Golden programs looping 10 million iterations (local, mutual, remote) at
    O0 and O2.
  - [ ] IR inspection shows the chosen tail-transfer form.

<a id="step-20"></a>

### 20. Support deep non-tail recursion

Backlog: F21. Depends on: [19](#step-19).

- Success criteria
  - [ ] Body recursion deeper than the native stack (for example building a
    1-million-element list) succeeds within the process budget.
  - [ ] Exceeding the budget produces the documented failure, not a native
    crash.
- Tests
  - [ ] Golden programs for deep body recursion and for the budget limit.

<a id="step-21"></a>

### 21. Lower list comprehensions

Backlog: F13, F16. Depends on: [19](#step-19).

Include filters, multiple generators, pattern generators, strict generators
(`<:-`) and zip generators (`&&`) as accepted by OTP 29.

- Success criteria
  - [ ] Results and evaluation order match OTP, including skipped non-matching
    elements and strict-generator errors.
  - [ ] Long inputs run in bounded stack.
- Tests
  - [ ] Golden programs for each generator kind, nested comprehensions and
    100k-element inputs.

<a id="step-22"></a>

### 22. Lower binary and map comprehensions

Backlog: F13, F16. Depends on: [21](#step-21).

- Success criteria
  - [ ] Binary generators/producers and map generators/producers match OTP,
    including partial bytes and duplicate map keys.
- Tests
  - [ ] Golden programs for each combination of list, binary and map generators
    and producers.

## F. Memory management

<a id="step-23"></a>

### 23. Extend the root inventory to the execution model

Backlog: F02, F03, F04, F08–F11. Depends on: [17](#step-17), [8I](#step-8i).

Phase C delivered the layout walker (8C), host handle roots (8E) and stack
roots (8F). Add the roots the step-17 model introduces: suspended frames or
continuations, in-flight error payloads and stack traces, and atom/module pins
held by heap cells.

- Success criteria
  - [ ] Every root owner of the step-17 model is enumerated by the collector;
    nothing outside the handle table, stack and listed owners holds heap words.
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

<a id="step-78"></a>

### 78. Publish the final implementation and validation boundary

Backlog: all. Depends on: steps 1–70 and any selected optional work.

- Success criteria
  - [ ] `00-finished.md`, `01-todo.md`, `arch.md`, `files.md`, contracts and
    examples reflect exactly what is implemented, validated, deferred or
    omitted.
  - [ ] README shows building and running a multi-process Erlang program as an
    executable.
- Tests
  - [ ] Fresh full gate on every available host; README commands executed as
    written.
