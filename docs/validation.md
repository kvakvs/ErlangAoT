# Validation

## Current baseline

Plan 11 step 1, 2026-10-03, commit `b1a471f`, Windows x64: clang-cl 23.1.2,
LLVM SDK 23.1.2 (`/MT`, `_ITERATOR_DEBUG_LEVEL=0`), Lizard 1.24.0, clang-tidy
22.1.8, OTP pin `21776803ecd1` with oracle OTP 29.1.1 / ERTS 17.1.

| Check | Result |
| --- | --- |
| Fresh combined Debug CTest | 125/125, zero skips (729 s serial; 85 s with `-j 16`) |
| Fast-mode CTest (`debug-fast`) | 122 tests, about 60 s |
| `check-quality` | Pass; Lizard CCN 10, clang-tidy over 258 production units |
| Opt-in OTP audit tests | 14/14 |
| `regenerate.py --corpus all --check` | 19/19 corpora reproduce |
| Foreign O0/O2 objects | 7 targets inspected (Linux x86/x64/ARM/AArch64, Windows x86/x64, Apple Silicon) |

## Running the gate

From an x64 Visual Studio developer shell with `C:\Program Files\LLVM\bin` on
`PATH` (automatic SDK selection reuses `thirdparty/`):

```powershell
cmake --preset debug --fresh -DBUILD_TESTING=ON -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build/debug
ctest --preset debug-fast                  # during development
ctest --preset debug -j 16                 # full mode, at feature completion
cmake --build build/debug --target check-quality      # changed files + dependents
cmake --build build/debug --target check-quality-all  # whole tree
```

- `ERLANG_AOT_TEST_MODE=fast` runs golden corpora at O0 positional plus O2
  specialization-off project, runs mutations once and skips `full_only` tests.
  Full mode (default) runs all driver/policy combinations.
- `check-quality`, `make format` and `make-format.bat` cover files changed since
  `HEAD` plus untracked files; `cmake/quality_scope.py` adds translation units
  that include a changed header. Changes to `.clang-tidy`, `cmake/` or production
  CMake select everything.
- Test programs carry no debug information (no PDBs); configure with
  `-DERLANG_AOT_TEST_DEBUG_INFO=ON` to rebuild them for a debugger. Nested
  native consumers link the parent build's runtime through
  `ErlangAoTRuntimeTargets.cmake` instead of compiling it again; only
  `runtime_link` builds the runtime standalone.
- Thresholds and suppressions are never raised to pass the gate.

## Fixtures and provenance

- Normal builds and tests need neither OTP nor its source checkout. Goldens were
  generated once from OTP and are committed with hashes; hash checks run before
  any fixture is used.
- Nineteen pattern/guard corpora hold 67,634 native expected values/errors and
  106 semantic acceptance rows. Each native corpus runs positional and project
  drivers at O0/O2 with specialization on/off, local and remote calls.
- Committed Erlang inputs are locally authored
  (`tests/fixtures/patternmatch/fragments/`, preprocessor `semantic/headers/`).
  A 2026-10-03 audit removed all copied OTP files; 60-token window comparison
  against 4,150 OTP Erlang and 1,190 C/C++ files found no remaining overlap
  besides a generated integer tuple. `fixture_sources` enforces isolation.
- Six end-goal program fixtures (`tests/fixtures/programs/`, plan 11 step 2)
  hold OTP stdout/exit-status goldens and today's compile diagnostics;
  `programs_compile` checks them OTP-free and
  `tests/compiler/programs/regenerate.py --check` reproduces them under OTP
  ([fixture map](../tests/fixtures/programs/README.md)).
- Term printing goldens (`tests/fixtures/printing/`, plan 11 step 4) hold OTP
  `~w` and `erlang:display/1` text for 9,542 values plus OTP stdout of compiled
  display calls; `tests/compiler/printing/regenerate.py --check` reproduces them
  ([fixture notes](../tests/fixtures/printing/README.md)).
- Executable golden cases (`tests/fixtures/executables/`, plan 11 step 8) are
  source directories plus one `golden.json` with OTP stdout/exit status and an
  authored stderr pattern; `tests/compiler/executables/run.py` links and runs
  each under the policy/driver matrix (CTest `executables_<case>`), and
  `regenerate.py --check` reproduces them under OTP
  ([case notes](../tests/fixtures/executables/README.md)).
- Regeneration and live audits are explicit:
  `-DERLANG_AOT_OTP_AUDITS=ON` and `tests/compiler/patternmatch/regenerate.py`
  ([instructions](../tests/fixtures/patternmatch/generated/README.md)).
  Nothing refreshes goldens or the pin silently.

## Test design

Behavior is tested through the real CLI, emitted objects, linked native consumers
and OTP goldens. Focused unit tests remain only where source cannot reach the
state, each with its purpose stated in the test:

- Injected budgets and allocation/IO faults (`project_limits`,
  `project_creation_failure`, `codegen_limits`, `codegen_write_failure`,
  `runtime_lifecycle_failure`, generated-call fault seams).
- Private ownership, invalid/stale handle and rollback invariants in the parser,
  semantic type graph and backend.
- Raw word validation and 32/64-bit term boundaries (`runtime_immediate`,
  `runtime_term_tag`, `abi_integers`).
- Diagnostic sink failures and the stable feature ID snapshot.

Filesystem capability cases (links, case aliases) report per-case skips; they
never stand in for a whole-test pass.

## Platform and sanitizer status

- Native generated-code execution: Windows x64 only.
- Linux, Apple Silicon and native 32-bit execution: pending (objects are only
  inspected).
- Compiler/frontend ASan, UBSan and LeakSanitizer: pending. The prebuilt Windows
  LLVM SDK conflicts with instrumented code (`annotate_string` 0 vs 1; earlier
  also duplicate rpmalloc/ASan allocator symbols). No check was disabled to
  bypass it.
- Runtime-only ASan passes on Windows with Release probes,
  `/EHsc /fsanitize=address`, `/MT`, Clang's ASan import library and static
  runtime thunk, and the ASan DLL on `PATH`.
- Historical macOS arm64 runs (2026-09-19/20) passed full Debug, C++26,
  ASan+UBSan, compiler-only and runtime-only builds for the parser and project
  stages; they predate the backend.

## History

Condensed from the former per-step records (originals in Git history up to
commit `2777c98`). Unless noted: Windows x64, LLVM 23.1.2, pin `21776803ecd1`,
oracle OTP 29.1.1 / ERTS 17.1. Test counts are full CTest passes with zero skips.

| Date | Milestone | Tests | Quality units | Notes |
| --- | --- | ---: | ---: | --- |
| 2026-09-19 | Parser phase VI (macOS arm64, OTP 29.1 `751f87b7`, oracle 29.0.5) | 46 | full | 344/344 productions witnessed; 10-file corpus |
| 2026-09-20 | Projects (macOS arm64) | 64 | full | Debug, compiler-only, ASan+UBSan |
| 2026-09-28 | Test migration to CLI workflows | 74/75 | — | Baseline 78/93; `parser_hardening` stack overflow later fixed with 8 MiB stack |
| 2026-09-28 | Windows gate repair | 75 | full | Lizard + clang-tidy clean |
| 2026-09-29 | Compiler milestone steps 1–46 | 103 | 182 | Compiler-only 80, runtime-only 16, runtime ASan 16 |
| 2026-10-01 | PG1 semantic matrix and evidence | 104 | — | Source hashes pinned |
| 2026-10-01 | PG2 failure channel | 108 | — | ABI rev 2 |
| 2026-10-01 | PG3 atoms | 109 | 189 | ABI rev 3 |
| 2026-10-01 | PG4 scoped bindings | 111 | 191 | 26 legality cases |
| 2026-10-02 | PG5 pattern semantics | 113 | 196 | 92 legality modules |
| 2026-10-02 | PG6 immediate matching | 114 | 199 | 34 calls |
| 2026-10-02 | PG7 immediate guards | 119 | 205 | 1,689 calls |
| 2026-10-02 | PG8 guard control flow | 120 | 207 | 2,075 calls |
| 2026-10-02 | PG9 ordered clauses | 121 | 208 | 1,020 calls |
| 2026-10-02 | PG10 body matches | 122 | 209 | 1,666 calls |
| 2026-10-02 | PG11 stable heap and roots | 123 | 213 | ABI rev 4 |
| 2026-10-02 | PG12 tuples/lists/strings | 125 | 221 | 4,801 calls |
| 2026-10-02 | PG13 arbitrary integers | 127 | 231 | 16,065 calls |
| 2026-10-02 | PG14 floats | 129 | 238 | 14,436 calls |
| 2026-10-02 | PG15 maps | 131 | 244 | 8,010 calls |
| 2026-10-02 | PG15a OTP-free goldens | 118 | 244 | 14 corpora, 49,959 values; audits opt-in |
| 2026-10-02 | PG16 bitstrings | 120 | 253 | 8,826 calls |
| 2026-10-03 | PG17 tuple records | 121 | 257 | 1,025 outcomes, 29 semantic cases |
| 2026-10-03 | PG18 guard catalog | 122 | 257 | 81 rows, 5,033 outcomes |
| 2026-10-03 | PG19 binding facts | 123 | 258 | 822 outcomes, 976 dominance checks |
| 2026-10-03 | PG20 closure | 124 | 258 | 19 corpora, 67,634 values, 1,969 seeded outcomes |
| 2026-10-03 | OTP source audit | 125 | 258 | Copied OTP files replaced by local fragments |
| 2026-10-03 | Plan 11 step 1 baseline | 125 | 258 | See current baseline |
| 2026-10-03 | Plan 11 step 2 program fixtures | 126 | 258 | Six OTP goldens; fast mode 123 tests; full `-j 16` 83 s |
| 2026-10-03 | Plan 11 step 4 term printing | 128 fast | 265 | 9,542 `~w`/display goldens; 154 compiled display calls in all policies; clang-tidy run with one job (concurrent runs crashed the tool on unchanged units) |
| 2026-10-04 | Plan 11 step 8 executable runner (phase B closed) | 138 (135 fast) | 272 | Cases `demo`, `exits` under eight policy/driver combinations; full `-j 16` 235 s |
| 2026-10-04 | Plan 11 step 8I classic heap (phase C closed) | 144 (140 fast) | 276 | Full `-j 16` 370 s: 143/144, `codegen_dependency` timed out at 120 s under load and passed alone in 29 s; Lizard-all 0 warnings; tidy-all passed with one job after a silent two-job tool exit |
| 2026-10-04 | Plan 11 step 9 `case` and `begin` | 146 (142 fast) | 114 changed | Fast 142/142; affected tests 7/7 in full mode; Lizard 0 warnings; tidy passed with one job after a silent two-job exit |
| 2026-10-05 | Plan 11 step 10 `if` | 147 (143 fast) | 114 changed | Fast 143/143; affected tests 8/8 in full mode; Lizard 0 warnings; tidy passed |
| 2026-10-05 | Plan 11 step 11 source raises | 148 (144 fast) | 124 changed | Fast 144/144; affected tests 22/22 in full mode; Lizard 0 warnings; tidy passed |
| 2026-10-05 | Plan 11 step 12 `catch Expr` | 149 (145 fast) | 277 | Fast 145/145; affected tests 25/25 in full mode; Lizard 0 warnings; tidy passed |
| 2026-10-05 | Plan 11 step 13 `try ... of ... catch` | 150 (146 fast) | 189 changed | Fast 146/146; Lizard 0 warnings; tidy passed |
| 2026-10-05 | Plan 11 step 14 `try ... after` | 153 (149 fast) | 2 changed | Fast 149/149; Lizard 0 warnings; tidy passed |
| 2026-10-05 | Plan 11 step 15 stack traces and `raise/3` | 154 (150 fast) | 129 changed | Fast 150/150; Lizard 0 warnings; tidy passed |
| 2026-10-05 | Plan 11 step 16 `maybe` (phase D closed) | 155 (151 fast) | 277 | Fast 151/151; full `-j 16` 155/155 in 259 s; Lizard-all 0 warnings; tidy-all passed |
| 2026-10-05 | Plan 11 step 18 recursive call graphs | 156 (152 fast) | 91 changed | Fast 152/152; affected tests 13/13 in full mode; Lizard 0 warnings; tidy passed |
| 2026-10-05 | Plan 11 step 19 explicit frames and tail calls | 157 (153 fast) | 278 | Fast and full CTest pass (with the step-20 case: 154/154, 158/158); clang-cl configure; Lizard 0 warnings; tidy passed |
| 2026-10-05 | Plan 11 step 20 deep body recursion and stack budget | 158 (154 fast) | 278 | Fast 154/154; full 158/158; Lizard 0 warnings; tidy passed |
| 2026-10-06 | Plan 11 step 21 list comprehensions | 159 (155 fast) | changed | Fast 155/155; affected tests full mode; Lizard 0 warnings; tidy passed |
| 2026-10-06 | Plan 11 step 22 binary and map comprehensions (phase E closed) | 160 (156 fast) | all | Fast 156/156; full `-j 12` 160/160 in 275 s; Lizard-all 0 warnings; tidy-all passed after fixing four new-code findings |
| 2026-10-06 | Plan 11 step 23 root inventory, `SafePoint`, live registers | 160 (156 fast) | 50 changed | Fast 156/156; Lizard 0 warnings; tidy passed |
| 2026-10-06 | Plan 11 step 26 collection from generated code | 161 (157 fast) | 63 changed | Fast 157/157; full `-j 12` 161/161 in 107 s; Lizard 0 warnings; tidy passed |

PG = pattern/guard plan step (archived in `.agents/00-finished.md`).
