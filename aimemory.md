# Research notes

## Shared IDE configuration — 2026-09-18

- `CMakePresets.json` schema 3 targets C++23 in build/debug and works with
  Visual Studio 2022 plus VS Code CMake Tools. No fixed compiler/generator/path.
- VS Code workspace settings enable the CMake Tools provider for cpptools;
  fallback include paths cover project and cloned pinned Boost.Parser headers.
- First configure requires the pinned Boost.Parser checkout or a local override
  in ignored CMakeUserPresets.json. Check compilation DB for -std=c++23 and Boost
  include on a configured host. Extension recommendations travel with Git.
- Validation: preset configure/build/test succeeds on macOS; all seven CTests pass.
  Compile database has C++23 for compiler units and Boost include for frontend.
  Required full-build Lizard and clang-tidy gate passes before commit.

## Makefile entry points — 2026-09-18

- Root Makefile adds build/test for macOS/Linux. Build always configures compiler,
  runtime and tests; test depends on build and fails on an empty CTest suite.
- BUILD_DIR defaults build/debug, BUILD_TYPE Debug, JOBS 2; CMAKE_ARGS forwards
  toolchain/dependency options. Explicit CMake parallelism avoids jobserver
  forwarding differences between macOS make, CMake, and the generated build tool.
- No dependency download or quality-gate bypass; CMake remains authoritative.

## Step 3 implementation — 2026-09-18

- PreprocessorSession emits OrdinaryForm/Directive/Diagnostic, owns reserved macro
  table + include/conditional stacks + feature/context state. Syntax only: no
  macro storage effects/expansion, include lookup, branch selection, CLI mode.
- Pure token-cursor envelopes preserve kinds/locations, arbitrary macro bodies,
  object-vs-zero-arity distinction. Duplicate formals deferred to step4; if/elif
  and error/warning term grammars deferred as planned. Includes accept strings
  and macro-leading operands pending expansion; no host filesystem lookup.
- Recovery drains lexical tokens to dot/EOF, forces progress after scanner errors,
  drains pending sigil tokens, and keeps failure latched across later valid forms.
- Misplacement detection is heuristic: line-leading structural names after arrow;
  exclude arbitrary attributes, replacement bodies and error/warning unary calls.
  Full expression ambiguity needs later parser; do not claim full epp behavior.
- Added pinned punctuation regression (#_ and Latin-1 standalone symbols) after
  inspecting OTP29 scanner while testing driver boundaries. Six golden fixtures.
- Validation: seven CTests pass in C++23, C++26, ASan/UBSan, with both live OTP29.1
  oracles executed. Fresh full compiler/runtime check-quality passes at unchanged
  CCN/cognitive thresholds; no project warning suppressions introduced.

## Step 2 implementation — 2026-09-18

- Source ownership/positions, diagnostics, incremental lexer complete; arbitrary
  decimal integers avoid a runtime dependency. OTP29 based floats included.
- Oracle built locally at build/otp29-install/bin/escript; set ERLANG_AOT_ESCRIPT.
  Both epp/scanner oracle tests execute; all six CTests pass in C++23/C++26 and
  ASan/UBSan. Fresh full-build check-quality passes without suppressions.
- Five lexical fixtures + exact OTP29.1 generated records run offline too.
- Encoding marker is case-sensitive coding, value Latin-1 case-insensitive;
  first marker in first two comment lines decides encoding. Scanner numeric
  suffix rejection deliberately covers ASCII only, matching OTP29 NAMECHAR.
- Next: step3 session/directive envelopes; semantics deferred per plan.

## Step 1 implementation — 2026-09-18

- Boost.Parser 1.90.0 checkout in build/deps/boost-parser, commit
  647cec66831407742a6ad78582f2a9f3cd7d44d3. Standalone include target avoids upstream
  CMake's mandatory Boost targets. Primary header SHA pinned; installed prefix works.
- Public parsable_iter accepts only character code units; static_assert documents
  rejection of struct tokens. Plan's explicit-token-cursor fallback chosen.
- Recursive character grammar tested via parser_probe. Optional epp harness checks
  exact OTP29.1; system OTP is 28. Built OTP29.1 inside ignored references/otp
  (configure generates in-source Makefiles); logs in build/otp29 and installed
  prefix build/otp29-install for reference validation.
- Clang opt-in enum analysis needs the Boost flags forward declaration annotated
  clang::flag_enum (valid zero/combinations); parsing/boost_parser.hpp supplies
  the semantic annotation without disabling diagnostics or modifying dependencies.
- Step 1: C++23 tests and both quality checks pass; optional oracle skips on OTP28.
  Runtime-only configuration/build succeeds with nonexistent Boost root.

## 2026-09-18 — Combined clean-commit quality gate

- User requires both Lizard and clang-tidy to pass for a clean commit. Installed
  clang-tidy 22.1.8 macOS arm64 wheel into .venv-quality and pinned requirements.
- .clang-tidy enables clang-analyzer-*, bugprone-*, performance-*, and cognitive
  complexity <=10; all reported warnings are errors. check-quality requires both
  project components and depends on check-complexity and check-clang-tidy.
- No Git hook installed; required pre-commit command documented in AGENTS/README.
- Clang-tidy's standalone wheel did not find Apple libc++ headers automatically.
  QualityToolchain.cmake supplies CMake's detected implicit includes and Apple SDK.
- Refactored CLI parsing with span-based consumption and parse_option/parse_output
  helpers. Maximum CCN now 10 (previous parse_options 21); cognitive check passes.
  Added option precedence, output consumption, and end-marker CLI regression cases.
- Validation: both default quality checks pass; C++23 build and CLI CTest (20
  scenarios) pass; pip check passes. Partial build's combined gate rejects the
  request. clang-tidy initially rejected cognitive complexity 24 at threshold 10.

## 2026-09-18 — Complexity tooling installed

- User requested an open-source cyclomatic complexity tool for quality control.
  Installed Lizard 1.24.0, pathspec 1.1.1, Pygments 2.21.0 in .venv-quality using
  Python 3.9.6; pinned tools/requirements-quality.txt and ignored environment.
- cmake -P cmake/CheckComplexity.cmake or optional check-complexity target scans
  compiler/runtime/abi C++, default CCN 10; normal builds independent of Python.
- Existing parse_options CCN 21 fails gate; run=8, main=2. No source refactor,
  suppression, or relaxed default. README documents baseline and report-only mode.
- Lizard is lexical, not Clang CFG analysis; template/new syntax counts need review.
- Validation: pip check passed; default standalone and CMake target correctly fail
  on CCN 21; standalone threshold override 21 passes; report-only mode exits 0.
  CMake configure/build and existing CLI CTest passed. Default remains 10.

## 2026-09-18 — Local OTP source and test discovery

- User requested a gitignored OTP checkout and preprocessor test discovery.
  Cloned --depth 1 --branch OTP-29.1 to references/otp; commit
  751f87b703fe5948607d08e82599ce644b772e76. Added /references/otp/ ignore rule.
- Inventory: .agents/01-pp-otp-tests.md. Main suite is stdlib/test/epp_SUITE.erl,
  many inline source fixtures; features live in erts/test/erlc_SUITE.erl and use
  OTP_TEST_FEATURES=true synthetic catalog. Compiler integration: compile_SUITE.
- Pin in 01-pp.md now resolved to local OTP-29.1; no build or suite execution.
- Concrete regressions: fun_type_arg distinguishes fun types vs end-delimited
  funs; include_local prioritizes nested header sibling over supplied include dir;
  test_if treats 42 as false and arithmetic evaluation failure as a skipped body;
  otp_8130 covers non-parameter ??B behavior. Do not invent stricter semantics.

## 2026-09-18 — Boost.Parser selected; preprocessor plan

- User selected Boost.Parser and requested a step-by-step plan based on the Erlang
  macros chapter. Saved `.agents/01-pp.md`; no code/dependencies implemented.
- Plan covers all chapter features in 13 steps with OTP oracle tests, token/source
  ownership, include resolver, contextual macros, and restricted guard evaluation.
- Pin exact OTP 29 before implementation: live macros docs reported 29.0.6 while
  expressions docs reported 29.1. Master epp is navigation only, not the baseline.
- Internal preprocessing tokens do not select stage interchange formats. Readers
  remain directory reservations. Proposed CLI preprocessing-check mode avoids
  prematurely committing to a public preprocessed text format.
- Verify Boost.Parser token-cursor extension in first spike; don't assume generic
  token ranges work with character primitives. Keep parser actions transactional.
- Proposed builtin compatibility values: OTP_RELEASE=29 and MACHINE='BEAM'; explain
  their meaning in a native compiler. Include guards can allow recursive includes,
  so don't reject every repeated active path. Runtime stays independent of Boost.

## 2026-09-17 — Initial CLI and CMake scaffold

- User requested main.cpp and CMake setup for macOS, with Windows notes. Implemented
  executable target `erlang_aot`, output name `erlangaot` following current request.
- CLI supports help/version, one output option, multiple inputs, and `--`.
  Usage errors exit 2; input/unimplemented compilation errors exit 1; help/version
  exit 0. Compilation never creates or overwrites output.
- Separate runtime static library has an empty placeholder translation unit.
  ABI interface target remains empty; no ABI or runtime functionality invented.
- C++23 default/26 selectable; CMake 3.28 minimum; no third-party dependencies.
  README and .agents/plan-windows.md document current behavior and deferred Windows work.
- Added CTest CLI behavior checks and required `.agents/arch.md` / `.agents/files.md`.
- Validation: Apple Clang 21 / CMake 4.4.2 on arm64 macOS. Combined C++23 build,
  runtime-only build, and compiler-only C++26 build passed. CLI CTest passed in
  C++23 and C++26 builds (14 scenarios each); git diff whitespace check passed.

## 2026-09-17 — Generated-code/runtime linkage clarification

- User allows any platform-compatible linkage and asks whether C++ linkage is preferable. Updated `00-plan.md` to remove the mandatory C-linkage boundary.
- Generated C++ can use ordinary C++ interfaces through shared headers and let Clang handle the ABI. Direct LLVM IR must match the ABI explicitly; C-linkage functions implemented in C++ remain a simpler option, not a requirement.
- Same architecture/OS alone does not establish compatibility. Initially use a selected compatible target toolchain/configuration and rebuild runtime plus generated code together when the ABI changes. C++ object/standard-library/exception ABI requirements apply when those features cross the boundary.

## 2026-09-17 — C++ project layout selected for planning

- User requested a preliminary C++ preprocessor/parser/compiler and separate C++ runtime in the same repository, two separate targets, CMake, modern C++ at least 20; save to `00-plan.md`.
- Created `00-plan.md` as a proposal, not an implemented scaffold. Main targets: `erlang_aot` executable (output `erlang-aot`) and `erlang_runtime` static library. Header-only ABI interface is not a third binary artifact.
- Proposed C++23 default with explicit C++26 selection on supported toolchains, CMake 3.28+ (newer if needed for C++26), required standard/no extensions. Host compiler and target runtime are independent; cross builds use separate build trees.
- Updated AGENTS.md's open questions with the user's C++ direction and preliminary layout. Earlier language-comparison recommendations below are historical and superseded by this request.
- Stage exchange formats/readers, detailed ABI and backend choice remain undecided. No build files or source directories created yet.

## 2026-09-17 — Implementation-language comparison

- Scope clarification from user: this discussion concerns only the ErlangAOT compiler tool's implementation language. Generated code will require a separate ERTS-like runtime with basic library functions, threading/scheduling, and event handling. Compiler output may be LLVM IR, C++, or another LLVM-ingestible representation. Do not rank compiler host languages by their suitability for implementing the target runtime.
- Revised compiler-only assessment: Erlang is a strong initial choice when OTP-assisted builds are acceptable, because native term transformations and existing OTP frontend infrastructure minimize compatibility work. Rust is a strong choice for a native compiler tool with typed IRs and eventual independence from OTP. C++ wins on direct LLVM internals integration; Java/Kotlin and Go remain legitimate compiler-only choices. No language is selected yet.
- User requested initial research comparing C++, Rust, Java/Kotlin, Go, and Erlang. No implementation language has been selected by the user; keep AGENTS.md questions open until a decision is made.
- Earlier recommendation (superseded in scope): Rust for compiler transformations, driver, and native runtime, with an OTP frontend adapter. This combined compiler/runtime assessment prompted the user's clarification above; do not reuse it as the compiler-only conclusion.
- Distinguish compiler host language from generated-program runtime. Java/Kotlin/Go/Erlang can emit native LLVM IR without imposing their host runtime on output, provided the target runtime is implemented separately.
- This is an Erlang frontend targeting LLVM IR, with native machine-code backends, rather than a new LLVM hardware target. LLVM supplies GC integration mechanisms, not the collector, scheduler, or mailboxes.
- Rust ownership does not replace Erlang GC. Moving GC, tagged terms, FFI, and low-level context management require carefully designed unsafe boundaries/handles. C++ avoids binding gaps but adds memory-safety burden.
- LLVM C API stability is best-effort; textual IR has no backward-compatibility promise. Pin an LLVM toolchain and compatible bindings. Clang availability alone does not ensure LLVM development libraries/headers are installed.
- OTP abstract forms are documented. Core Erlang/cerl and primops have release compatibility caveats. Pin OTP 29 and avoid committing to a stage interchange format/parser during initial language research.
- Existing pure Erlang applications still depend on runtime BIFs and OTP services. Proposed feasibility test: bounded-stack tail recursion, spawn/send/selective receive, scheduling fairness, and forced GC preserving live values; compare behavior against OTP 29.
- Firefly (formerly Lumen) is relevant prior art with a Rust runtime/native compilation architecture, but its repository was archived June 10, 2024; study it as reference rather than assume maintained OTP 29 support.

Primary sources consulted:
- https://llvm.org/docs/FAQ.html
- https://llvm.org/docs/DeveloperPolicy.html
- https://llvm.org/docs/GarbageCollection.html
- https://www.erlang.org/doc/apps/compiler/compile.html
- https://www.erlang.org/doc/apps/compiler/cerl.html
- https://www.erlang.org/doc/apps/erts/absform.html
- https://www.erlang.org/doc/apps/erts/garbagecollection.html
- https://www.erlang.org/doc/apps/stdlib/epp.html
- https://github.com/TheDan64/inkwell
- https://github.com/GetFirefly/firefly
- https://github.com/bytedeco/javacpp-presets/tree/master/llvm
- https://github.com/llir/llvm
- https://go.dev/doc/gc-guide
- https://kotlinlang.org/docs/native-memory-manager.html

Preprocessor progress: steps 4–6 now have semantic implementation and focused tests.
Shared literal-term parsing supports initial macro values; subsequent commits connect later directives.
