## 2026-09-19 — Parser Phase II execution

- Step 5 reuses decoded lexer values; flat typed Tuple/List/Group/BinarySigilLiteral
  nodes, checked children, adjacent strings and OTP build_sigil rules. Depth limit
  defaults to 256. Phase II fixtures are in a subdirectory to retain Phase I corpus.
- Phase II dump is private, exhaustive, strips groups and normalizes list spines;
  native shape/provenance assertions protect the richer AST metadata separately.
- Step 4 commit was d84bcb2. Continue steps 5–7 with separate clean quality commits.

## 2026-09-19 — Parser and typed AST plan

- `.agents/02-parser.md`: requested planning only, six phases/18 ordered steps;
  every future step requires its own commit after fresh full check-quality.
- Reuse expanded OrdinaryForm tokens, existing Lexer/source/provenance and oracle
  infrastructure. Extract small cursor/syntax/operator helpers; do not promote
  preprocessor Expr kind/children or evaluator into the general syntax AST.
- OTP29.1 grammar includes native records, strict/zipped generators, and multiple
  list/map comprehension templates. Attribute build actions and nominal type
  declarations are part of parsing. Inspected generated stdlib erl_parse.erl,
  compiler compile.erl/v3_core.erl, and stdlib erl_expand_records.erl as requested;
  preserve final module feature metadata and defer transforms/lint/expansion/Core.
- Pattern/guard grammar intentionally permits some later lint failures; typed
  syntax must preserve that. compr_assign checks live in erl_lint; add feature
  context handoff because current public preprocessing events do not expose it.
- Plan proposes module arenas/category IDs, transactional forms, source tables,
  typed node variants, parse-check CLI; no code implemented by this task.

## 2026-09-18 — Semantic preprocessing (steps 4–13)

- User explicitly requires separate buildable commits per step; preserve their pre-existing
  AGENTS.md formatting-instruction edit. Each commit requires fresh full check-quality.
- Native preprocessor and CLI implemented. See docs/preprocessor.md for policies and files.
- Oracle is build/otp29-install/bin/escript, built from ignored references/otp at OTP-29.1
  commit 751f87b703fe5948607d08e82599ce644b772e76. Default system OTP is older.
- Full Boost headers under build/deps/boost_1_90_0 supplement standalone Parser. Compiler only.
  Use eager cpp_int values; in-place arithmetic avoids expression lifetime/analysis issues.
- Pinned epp peculiarities: includes require literal strings; object macro bodies rescan
  before joining caller tokens; static cycles include unused arguments; variable LINE and
  FUNCTION macros bypass undef table; undefined contextual placeholders satisfy defined()
  but not ifdef; feature query macros expand to comparison expressions.
- OTP epp crashes on record construction conditions and binary/fun predefinitions through
  erl_parse:tokens. Native handles these explicitly; records remain a later parser concern.
- Native debug, C++26, sanitizer, runtime-only and quality results are recorded with final
  completion. Other host platforms and full upstream Common Test remain outstanding.

# Research notes

## 2026-09-19 — Parser Phase I execution

- Step 1: 14 authored corpus fixtures, complete pinned grammar/action inventory,
  private normalized raw/epp/lint records and exact-version escript replay.
- Offline parser_reference checks native lexer parity and reference integrity;
  it does not yet claim native AST parity. Live parser_oracle covers all records.
- Initial full debug suite: 12 CTests passed; missing/OTP28 oracle skips verified.
- User committed the parser plan during this turn (7987ef0); preserve that commit.
- Step 1 committed bd11eeb after full quality passed. Step 2 extracts cursor,
  syntax/diagnostic construction and contextual infix metadata. Condition adapter
  uses OTP precedence values with the same restricted operators and acceptance.
- Step 2 committed 3bf8682 (13 CTests plus full quality). Step 3 introduces typed
  module arenas with retained identity and per-slot generations; rollback cannot
  resurrect IDs. Only literals/variables, module/file attributes and explicit
  ZeroArgumentFunction exist until later grammar steps. Origins are owned per form.
- Step 3 committed 3b5406b after 14 CTests, sanitizer subset and fixed full quality.
- Step 4 implements ParserSession/raw form and parse_module APIs, module/file attributes,
  zero-argument single-scalar functions, diagnostic latching/rollback, bounded work,
  immutable per-form/final feature context. No CLI mode until step 17.
- Phase I native AST oracle exposed include-return file line mismatch: OTP scan_dot
  consumes one whitespace after dot, so only an immediate LF increments return line.
  Fixed State::scan resume_line; mixed-LF/CRLF include fixture now compares exact file attrs.
- Final Phase I validation: 17/17 CTests in debug C++23, C++26 and ASan/UBSan,
  with live OTP29.1 tests executed; runtime-only succeeds with absent Boost roots.
  Full Lizard/clang-tidy passes unchanged thresholds. Host evidence: macOS arm64.

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

- Step 5 final verification: 20 CTests, five sanitizer parser suites, missing/wrong
  OTP skips, full fresh Lizard/clang-tidy passed. New child validation explicitly
  checks active_ before dereferencing (tidy optional-access analysis).

- Step 5 committed a57d8e9. Step 6 adds Pratt parsing over shared metadata, including
  prefix reuse in preprocessing; calls 750 bind below nonassoc remote colon 800.
  Match/catch/call/remote have dedicated payloads. Test-only enum spelling mapping
  stays independent of production descriptors. Left chains iterate, right/unary
  chains consume the depth budget. Step 6 fixtures compare both precedence orders.
- Step 6 verification: fresh full build, all 20 CTests, preprocessing rerun after
  shared prefix migration, five sanitizer parser suites, full quality gate passed.

- Step 6 committed cb556d6. Step 7 replaces ZeroArgumentFunction with Function and
  typed clauses/optional nonempty GuardSyntax. PatternSyntax arena stores either
  RestrictedPattern or PatternCandidate wrapping ExprId; patterns join node budgets
  and transactions. pat_expr context excludes root calls/catch/send/short-circuit/
  remote, while container children still use expr. Parentheses preserve context.
- Candidate wrapper/storage is implemented now; case/receive grammar remains step10.
  Function argument builders reject candidates. Guard expressions intentionally
  preserve parse-valid/lint-invalid syntax; separate duplicate forms are accepted.
- Step 7 fixture guard_alternative(X) when true; f(Y) -> ok is parser-valid: the
  semicolon is still within the guard. Do not misclassify it as a clause mismatch.
- Phase II final verification: all 21 tests pass C++23/C++26/ASan+UBSan with live
  OTP29.1, runtime-only build passes, full fresh Lizard+clang-tidy passes. Step7
  complete after its required commit; resume future work at step8. No code-gen/CLI
  parse-check/control-flow/map/record/general-binary implementation is claimed.

## 2026-09-19 — Parser Phase III execution

- Step8: map/record expressions and patterns reuse token/Pratt infrastructure.
  Structural postfixes run before general Pratt continuation: map on expr_max/map;
  local record on expr_max/record; qualified/inferred record on expr_max only.
  Calls or mixed chains need grouping. Pattern roots disallow postfix update/access.
- Record identities use explicit unresolved local / qualified native / inferred
  variants; variable-looking/reserved names become atoms only in record_name.
  # _{} is unresolved local underscore, unlike #_{}. Index name must be an atom.
- Added phase3 fixtures via parameterized phase2.cmake rather than a copied harness.
  Node source/child validation extends to nested map/record fields and identities.
- Step8 verification: fresh full C++23 build, 24 CTests including live OTP29.1,
  seven sanitizer parser suites, full Lizard/clang-tidy gate passed before commit.
- Step8 committed d17cda4. Step9 adds Bitstring/BinarySegment/BinaryModifier, preserving
  optional size/types, ordered unknown/duplicate modifiers and big integer parameters.
  bit_expr = one optional prefix + expr_max; size = expr_max. Bare calls/maps/records/
  infix/negative sizes fail; groups re-enter expr. Use primary directly, not structural.
- BinarySigilLiteral removed: binary sigils lower to ordinary StringLiteral/utf8
  segments, modifier origin=prefix, string origin=string token. Historical test dump
  binary_sigil label retained for identical abstract shape (including explicit syntax).
  enter() shares recursion guard with bit_primary; flat segments/types remain iterative.
- Step9 final verification: all 25 CTests pass C++23/C++26/ASan+UBSan with live
  OTP29.1. Runtime-only builds and absent/OTP28 live skip paths pass. Full fresh
  Lizard/clang-tidy passed before step9 commit. Resume future work at step10.

- Added run-macos.sh: use existing make build/environment overrides, scan selected
  build/bin recursively for newest executable erlangaot, exec with "$@". Keep caller
  cwd and compiler stdout; build diagnostics go to stderr. Verified bash syntax,
  actual --version, and --preprocess-check from another cwd with a spaced filename.

- Boost discovery now accepts installed Boost >=1.90, including brew --prefix boost
  on macOS. Standalone Parser without version.hpp still uses the 1.90 checksum.
  Explicit roots, CMAKE_PREFIX_PATH, and local build/deps fallbacks remain supported.
- Current environment has Homebrew Boost 1.92; old local build/deps and pinned OTP
  installation are absent. Fresh/global build passes 18 native/offline CTests, with
  seven optional live OTP tests skipped. Runtime-only and old-version/bad-checksum
  configure rejection checks passed. Full Lizard/clang-tidy passed with Boost 1.92.
  User committed the launcher as 09c9d31 during this task; preserve that commit.
  End-to-end run-macos.sh --version also passed with the installed Boost.
- CMake build preset `debug` runs two jobs in parallel; callers can override it with
  `cmake --build --preset debug --parallel N`. Makefile builds retain `JOBS=N`.

## 2026-09-19 — Preprocessed source CLI

- `--print-pp` streams expanded forms to stdout via public `print_preprocessed` in
  `src/printing/source.cpp`. Shared token formatting moved into `printing/token_text.*`;
  epp macro stringification stays unchanged. Original sigil body spans are intentionally
  retained (raw/triple semantics), while other tokens use decoded canonical values.
- Diagnostics/exit status share `--preprocess-check`; combining flags prints source;
  explicit `-o` conflicts. No AST printing added in this smaller task.
- Validation: full Debug build, 19 native/offline CTests passed (seven optional OTP
  oracle tests skipped); fresh full Lizard/clang-tidy quality gate passed. CLI,
  printing round trips, and preprocessor golden tests rerun after the tidy fix.

## 2026-09-19 — Installed Erlang discovery

- Native compiler test builds now require host OTP >=29 via `ErlangDependencies.cmake`
  and an escript version probe. Prefer Homebrew's prefix on macOS; retain explicit
  ERLANG_AOT_ESCRIPT overrides. Runtime-only/no-tests/cross builds bypass discovery.
- This host's PATH/asdf selects 28.1.1, while Homebrew provides 29.0.5. Debug cache
  was rediscovered with `-U ERLANG_AOT_ESCRIPT` and now selects Homebrew. Confirmed
  OTP 28 fails a full configure, and nonexistent escript fails the version probe.
- Oracle adapters now accept >=29 instead of requiring exact 29.1; missing runtimes
  and mismatches fail rather than skip. Checked-in 29.1 baseline records are unchanged.
- Debug build and all 26 CTests passed on Homebrew OTP 29.0.5, with no skips.

## 2026-09-19 — AST tree CLI

- `--print-ast` now exposes the current grammar through one preprocessing/parser
  pass. Public print_ast is implemented by exhaustive visitors in src/printing/tree*;
  scalar fields stay on one line and named/indexed children preserve source order.
  Iterative traversal caps indentation at 64 levels, then emits explicit depth labels.
- Combined --print-pp prints each input's source before its AST. Recovered forms print
  even on errors; parser/preprocessor diagnostics appear once on stderr and exit1 is
  latched. Dedicated diagnostics-only parse-check remains future work.
- Literal spelling reuses token_text(kind,value); operator spelling reuses parsing
  metadata. Tests cover exact layout, all currently parsed node families, strings,
  destroyed source sessions, empty modules, 9000-operator chains and CLI recovery.
- Validation: Debug build, all 27 CTests (including live Homebrew OTP 29.0.5),
  clang-format, and full Lizard/clang-tidy quality gate passed.
## 2026-09-19 — Phase IV execution

- Step10 adds shared ast/clauses.hpp, typed begin/case/if/receive, candidate branches,
  constructor/source checks and exhaustive public/private printing. Function sequences
  and optional guards are reused. expect now consumes keyword delimiters but rejects
  form dots. Phase4 oracle fixtures use installed OTP29.0.5; pinned29.1 grammar inspected.
- Each PhaseIV step requires a separate commit after fresh full quality checks.
- Step10 validation: all 30 CTests and fresh full quality/format checks pass on macOS
  arm64 with installed OTP29.0.5. No preprocessor behavior change was required.
- Step10 commit d22c6d9. Step11 implements fun/try/maybe; catch reasons are restricted
  per pinned grammar, not general expression candidates. Catch defaults stay omitted
  in the AST and normalize only in oracle output. Function/fun constructor checks are
  now shared through Children::function_clauses. Keyword-state handling stays in PP.
- Step11: all 31 CTests plus additional alternative fixtures and full fresh quality
  passed; formatting clean. No semantic lowering or feature reinterpretation added.
- Step11 commit e7e5f16. Step12 adds typed list/map/binary comprehensions and flat
  simple/zipped qualifier variants. Aggregate prefixes are parsed once; binary
  templates are expr_max, binary generator roots must start with expanded << tokens
  (sigils and groups reject). Context flags prevent direct pattern comprehensions.
- Match qualifiers remain FilterQualifier/MatchExpression with source feature context;
  no compr_assign legality/binding transformation occurs in the parser.
- Corrected the lint oracle to request epp extra metadata and forward {features,...}
  to erl_lint like compile.erl. Enabled assignment now lints successfully, disabled
  assignment fails; both remain syntax-success cases. Prior fixture projections unchanged.
- Step12 validation: full Debug build, 32 CTests, all five parser oracle suites replayed
  after the lint fix, fresh full Lizard/clang-tidy and formatting pass. Separate
  build/phase4-sanitize ASan+UBSan passed parser_control/parser_exceptions/
  parser_comprehensions/printing_ast. CLI tree output checked across all new families.
- PhaseIV complete; future plan work resumes at step13 (attributes/declarations).

## 2026-09-19 — Phase V execution

- Step13 adds TermId arena/rollback/visitor access and typed attrs/records/docs.
  Most names (including export_type/compile/behaviour/on_load) intentionally use
  GenericAttribute because erl_parse applies literal normalization only to them.
- Private TermNormalizer traverses the expression AST with a strict whitelist;
  Name/Arity normalizes inside lists/tuples/map values, never map keys or binaries.
  Reuses Value comparison/numeric operations and extracted append_literal_bits;
  no parser expression/guard evaluator or ordinary calls are executed.
- Temporary ordinary-attribute expressions are reclaimed after conversion; doc
  metadata can retain equiv calls, record defaults remain expression IDs.
- Raw doc file path API test avoids epp's separate documentation-file expansion.
- Baseline pinned29.1; new step13 fixture records generated on installed29.0.5.
- Step13 validation: all 35 CTests pass, including live OTP suites; fresh full
  Debug compiler/runtime Lizard and clang-tidy pass; formatting and diff checks pass.
- Step13 commit 19b4bbc. Step14 adds TypeId arena, transaction counts, ownership
  checks and explicit type grammar (top_type versus type, separate from expression).
  Shared type operator metadata is used only within its type context. Applications
  classify builtin arities from pinned erl_internal; unknown local types remain local.
- Mixed typed/untyped record fields reuse defaults; alias/opaque/nominal declarations
  check variable parameters (excluding _) but defer type legality/bindings. Attribute
  envelope handling now also accepts parenthesized heads before outer ::/comma.
- Step14 validation: fresh full Debug compiler/runtime Lizard+clang-tidy pass;
  all 36 CTests pass across the full run and sequential rerun of two timed-out
  legacy OTP suites. New Phase V native/golden/live suites passed on first run.
  Formatting and diff checks pass.
- Step14 commit e8049bc. Step15 implements Specification/Signature/TypeConstraint,
  reusing FunType parsing and TypeId children. Legacy is_subtype(V,T) normalizes
  with a retained syntax-style flag; only parser checks are enforced, so differing
  overload arities and later arbitrary-argument signatures remain accepted.
- Grouped untyped record tuples are supported; grouped typed tuples reject per yrl.
- OTP record/type_spec helper input and first (...) signatures raise function_clause
  or badmatch. Dedicated .builder-reject raw oracle mode records this separately;
  native parser reports a syntax diagnostic and recovers. No epp server crash needed.
- phase5/coverage.tsv supplements immutable historical matrices with 90 concrete
  production-to-fixture rows. Shared oracle runner validates every linked path.
- Step15 validation: all 37 CTests pass sequentially, including every live OTP
  suite; fresh full Debug compiler/runtime Lizard+clang-tidy, formatting and diff
  checks pass. ASan+UBSan build/phase5-sanitize passes attributes/types/specifications/
  printing_ast. Historical attributes/types sources also match the native projection.
- Phase V is complete.
- Step16: shared Delimiters replaces macro-private Balance; expected/opener fields
  enrich syntax diagnostics. Work defaults16m, depth clamp512, printer visits4m.
  Reject invalid chained literal divisions before recursive normalization (stack fix).
  900 fixed-seed mutations replay twice; stress flat12000, wide5000/qualifiers3000.
  Debug regression run plus corrected hardening rerun pass; ASan/UBSan hardening,
  mutations and printing_ast pass. Native stress ~0.7s on this macOS arm64 host.
  Fresh full Debug Lizard/clang-tidy and formatting pass; step16 ready to commit.
