# Current working memory — 2026-09-19

- Active task: execute all 22 steps of .agents/03-project.md with separate commits
  and full gates per step; no subagents authorized. Step 1 passes all46 + quality.
  Gate helper /tmp/erlangaot-project-gate.sh takes step number; logs /tmp/project-stepN-*.
  Step2 prepared in /tmp/project-step2.py; pinned toml++3.4.0 downloaded/extracted
  to ignored build/deps (network escalation approved), archive hash in that script.

- User requested Phase VI of .agents/02-parser.md. Steps16/17 committed separately:
  0ff42f4 hardening; 6c58b4d parse-check/driver/API docs. Step18 local closure validated
  and ready for its commit. Full plan remains partial until unavailable hosts execute.
- Every numbered step requires a separate commit after fresh cmake --preset debug,
  full compiler/runtime check-quality (Lizard CCN10, clang-tidy cognitive10), formatting
  and relevant tests. No new suppressions or threshold changes. No subagents authorized.
- Build/debug uses Apple Clang21, macOS26.6.2 arm64, CMake4.4.2, Boost1.92 Homebrew.
  Baseline language C++23; C++26 selectable. Usually build --parallel2.
- CMake chooses /opt/homebrew/opt/erlang/bin/escript (OTP29.0.5). Bare erl/escript
  uses older asdf OTP28; don't use it. References/otp is ignored OTP29.1 revision
  751f87b703fe5948607d08e82599ce644b772e76; not a normal compiler dependency.
- Formatter: /Library/Developer/CommandLineTools/usr/bin/clang-format.
  Quality .venv-quality/bin tools. Focused tidy needs ALL includes and sysroot from
  build/debug/QualityToolchain.cmake. Full quality takes minutes; log to /tmp.
- Never rebuild a target while CTest is executing that target: causes transient
  oracle/native output mismatches. Independent build directories are fine.
- CLI has --preprocess-check/--parse-check/--print-pp/--print-ast, shared per-input driver.
  Public --print-ast was explicitly user-requested before step17; retain it despite
  original plan's private-dump instruction. Checks never write outputs; -o conflicts.
- AST is syntax only: closed expr/pattern/term/type/form variants and flat arenas,
  checked owner/generation IDs, transactional whole-form rollback, source origins and
  immutable per-form/final features. Grammar success deliberately permits lint failures.
- Shared Lexer is authoritative; parser consumes expanded tokens, never print/re-lex.
  Shared parsing helpers: cursor, syntax, diagnostics, operators, delimiters.
  Attribute normalization reuses private PP exact values/comparison/bits, not guards/calls.
- Native record forms, multi-template comprehensions, strict/zipped generators and
  compr_assign syntax follow pinned29.1 grammar; feature/lint legality is deferred.
  Catch reasons are restricted patterns; case/of branches are candidate patterns.
- Specs retain first-signature arity and overload differences until semantics.
  Literal TermIds and TypeIds remain distinct from expressions. Documentation equiv
  retains calls; parser does not read doc files. Existing PP has its own policy.
- Hardening defaults: nodes1m, form tokens1m, total4m, depth256 clamped512, work16m,
  diagnostics1000+exhaustion. Printer visits4m, iterative and bounded indentation.
  Invalid long f/1/1 attributes reject BEFORE recursive normalization (stack fix).
  No claim of wall-clock/linear bounds. Fixed900 mutations run twice, flat12000 stress.
- Step16 all39 tests pass across full run+corrected test rerun; sanitizer hardening/
  mutation/printing pass. Step17 all40 pass, sanitizer CLI/consumer pass. Both quality gates pass.
- Step18 measured coverage uses temporary instrumented pinned Yecc grammar, not seed
  inference: 344 ordinary productions observed, 79 SSA exclusions. Five missing cases
  added in phase6. Historic14 positives now have native/live AST comparison; old hashes untouched.
- Source corpus: lists/maps/sets/erl_scan/beam_ssa/beam_asm plus beam_ssa/beam_asm/
  beam_opcodes/beam_types headers. Explicit compiler include and stdlib/kernel app roots,
  maybe_expr enabled/compr_assign disabled, COMPILER_VSN="parser-compatibility".
  Manifest pins source hashes and script rejects modified relevant checkout paths.
- Root inventory contains literal semicolons AND unmatched bracket grammar tokens;
  CMake lists require encoding both before newline-to-list conversion.
- Coverage oracle allows function_clause/badmatch only for named .builder-reject inputs.
  Native returns recoverable diagnostics for these OTP builder crashes; no blanket skips.
- Shared cpp_int code prefers eager values and in-place operations; string constructor
  static analyzer false positives avoided by reusing existing literal_value(Token).
- Historical PP nuances: object bodies rescan before joining caller; include-return
  logical line advances only for immediate LF after dot; exact map keys retain types.
- Step18 builds: build/phase6-cxx26 (full), build/phase6-compiler-only,
  build/phase6-runtime-only, build/phase5-sanitize (full ASan+UBSan), build/debug (full).
  Tests/logs /tmp/phase18-*. Host matrix must leave Linux x86/ARM and Windows x86 pending.
- Final step18: all46 Debug and compiler-only tests pass in full runs. C++26 and
  ASan/UBSan all46 pass across full runs+focused historical/corpus reruns after
  fixing CMake list escaping; no sanitizer findings. Runtime-only build passes.
  Fresh full Debug Lizard/clang-tidy pass. docs/parser-validation.md has commands,
  exact scope and host matrix; no full upstream CT or cross-host claim.
