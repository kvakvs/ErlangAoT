# Research notes

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
