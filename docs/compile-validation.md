# Compiler milestone validation

The 2026-09-29 milestone uses Windows x64, Clang/LLVM 23.1.2, installed OTP 29.1.1
and the unchanged official `maint-29` source pin recorded in
[OTP reference instructions](otp-reference.md). Exact tool locations and ABI
settings are in the [compilation contract](compile.md#sdk-prerequisite).

| Configuration / check | Passing | Failing | Skipped | Scope |
|---|---:|---:|---:|---|
| Fresh combined Debug | 103 | 0 | 0 | Full CTest inventory, including runnable examples |
| Compiler-only Debug | 80 | 0 | 0 | Frontend, semantic, IR/object, resource and artifact workflows |
| Runtime-only Debug | 16 | 0 | 0 | Default Debug CRT/iterator checks, including allocation-failure sweeps |
| Runtime-only ASan Release | 16 | 0 | 0 | Lifecycle, memory, dispatch, registration and separate consumer |
| Full Lizard / clang-tidy | pass | 0 | 0 | All 182 original production compilation commands; existing thresholds |
| Foreign O0/O2 objects | 7 targets | 0 | 0 | Linux x86/x64/ARM/AArch64, Windows x86/x64, Apple Silicon |

The [complete CTest name inventory](compile-tests.txt) contains 103 tests: 35 parser,
33 codegen, 14 runtime, five preprocessor, four semantic, four project, two ABI,
and one each for CLI, frontend, printing, OTP, scanner and lexer. Native generated
code has current Windows x64 evidence only. Linux, Apple Silicon and native 32-bit
runtime/harness execution remain pending. No foreign object is executed by inspection.

Reproduce the combined gate in an x64 Visual Studio developer shell with the
existing SDK selected (adjust the installed prefix for another machine):

```powershell
cmake --preset debug --fresh -DBUILD_TESTING=ON -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DLLVM_DIR=F:/Projects/ErlangAoT/thirdparty/clang+llvm-23.1.2-x86_64-pc-windows-msvc/lib/cmake/llvm -DERLANG_AOT_DOWNLOAD_LLVM=OFF
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
cmake --build build/debug --target check-quality
ctest --test-dir build/debug --show-only=json-v1
git diff --check
```

The local validation selects pinned clang-tidy 22.1.8 via `CMAKE_PROGRAM_PATH`;
Lizard 1.24.0 runs from `.venv-quality`. Neither checks nor thresholds were disabled.
Each numbered step 40–46 has a separate commit and passing fresh shared gate.
Local command logs/JUnit/inventories live under ignored `build/compile-steps/`;
the [archived compiler ledger](../.agents/00-finished.md#compiler-validation-history)
preserves historical results.

`codegen_differential` checks 150 fixed/seeded native calls against OTP and an
independent evaluator in four policies, each run twice. `codegen_measurements`
adds a 255-argument/64-member-union case and writes timing/size evidence to
`build/debug/tests/compiler/codegen/measurements/Debug/measurements.json`.
Timings are descriptive. Source O2 enabled/disabled bytes match; synthetic guarded
fixtures verify hits, misses, rollback and deterministic caps without claiming
new source guard support. See [specialization evidence](specialization.md).

Full compiler ASan configuration was attempted and rejected by the installed SDK's
MSVC STL annotation ABI (`annotate_string` 1 versus 0). This is unavailable coverage,
not a passing sanitizer run or skipped CTest. No annotation suppression or replacement
SDK was used. Full compiler/frontend ASan, UBSan and LeakSanitizer remain pending.
The runtime-only ASan run uses Release probes, `/EHsc /fsanitize=address`, `/MT`,
Clang's installed ASan import library and static runtime thunk, with the DLL on PATH.
The SDK itself and emitted Erlang code are not instrumented by that runtime-only run.

The first runtime-only Debug OOM sweep exposed allocations inside MSVC STL noexcept
constructors/moves. Catchable container construction and key/name copying fixed the
failure; the final 16/16 run retains Debug iterator checks and allocation assertions.
Small-ceiling and partial-write injections remain intentional test exceptions; the
[migration ledger](test-migration.md) explains why public source cannot replace them.

Pattern/guard step 7 adds immediate service/guard execution, 1,689 differential calls, 31 resolution cases and four injected-service workflows. Fresh combined Windows x64 Debug: 119/119 tests and full Lizard/clang-tidy over 205 units. See [step-7 validation](patternmatch-step7-validation.md); other native hosts and new sanitizer runs remain unclaimed.

Pattern/guard step 8 adds comma/semicolon guards and strict/lazy boolean execution, with 2,075 differential calls, structured badarg payloads, word joins, both-width objects and four strengthened fault workflows. Fresh combined Windows x64 Debug: 120/120 tests and full Lizard/clang-tidy over 207 units. See [step-8 validation](patternmatch-step8-validation.md). Ordered clauses and other native runners remain pending.

Pattern/guard step 16 adds rooted immutable bitstrings, numeric/UTF segments,
checked matching cursors, retained tails, queries and structural comparison.
Project-owned goldens retain 8,826 expected calls per native policy; eleven added
semantic cases validate construction and modifier boundaries. Fresh Windows x64
OTP-free combined build: 120/120 CTests and all 253 production quality units pass.
Five suite parses and three target object checks are separate from execution.
Other native hosts/32-bit and new sanitizer runs remain unclaimed. See
[step-16 validation](patternmatch-step16-validation.md).

Pattern/guard steps 17–20 complete ordinary tuple records, the admitted guard
catalog, conservative binding/proof inference and final seeded/provenance closure.
Fresh Windows x64 Debug: 124/124 OTP-free tests and all 258 quality units pass.
Nineteen owned corpora retain 67,634 native outcomes in all eight driver/policy
combinations, plus 106 separate semantic rows. The documented remote classification
example passes all four policies. [Final validation](patternmatch-step20-validation.md)
records source/tool identities, scoped backlog closures and remaining platform gaps;
historical native/sanitizer records retain their original scope.
