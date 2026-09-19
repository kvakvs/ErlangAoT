# Current working memory — 2026-09-20

- Active task: implement all22 steps in .agents/03-project.md, separate commits
  after each full gate. Steps1–17 committed (latest065edc4) plus2539c2a Homebrew follow-up;
  step18 applied, gate running session28420 /tmp/project-step18-* logs.
- User steering: C++23 requirement; no C++26 checks for project work. Existing
  optional standard selector is unchanged. No subagents authorized.
- Scripts /tmp/project-step19.py through step22.py remain UNAPPLIED in main.
  Scripts through22 were tested in /tmp/erlangaot-project-preview; project/CLI tests
  and documentation shell examples pass. Preview21 adds project_hardening, 64th test.
  Must still apply/review/format/full gate/commit each main step in order.
- Step18 uses NewProjectFilename strong wrapper, std::ios::noreplace exclusive
  creation; step19 wraps arguments. All future production TUs passed preview tidy
  and Lizard. Step16 removes redundant direct executable frontend link.
- Step21 prepared docs at /tmp/project-validation.md with RESULTS_PENDING marker.
  Main build/project-sanitize and build/project-compiler-only have prewarmed C++23
  builds, but final code must be reconfigured/rebuilt/fully tested. Runtime-only
  final check must use absent TOML root. Linux/Windows host evidence stays pending.
- Step22 script writes README/docs/projects/example and clarifies matcher budgets.
  /tmp/project-doc-checks.py uses pip._vendor.tomli with .venv-quality/bin/python
  for TOML blocks and relative links. /tmp/project-validation.md date now2026-09-20. /tmp/project-doc-examples.py
  executes all relevant documented shell blocks; use once against fresh demo dirs.
- Gate helper: sh /tmp/erlangaot-project-gate.sh N freshly configures full Debug,
  builds, runs all CTest parallel2, then full check-quality and git diff --check.
  Never modify this helper while it runs. Full quality takes several minutes.
- Formatter /Library/Developer/CommandLineTools/usr/bin/clang-format. Focused tidy
  helper /tmp/project-focused-tidy.py supplies configured system includes/sysroot.
- Git commits require require_escalated with git prefix; authorized by plan.
  .agents edits require apply_patch. DO NOT stage/change/revert user Makefile clean
  target edit. Stage explicit own paths, track plan ledger/files/architecture.
- TOML private3.4.0 now explicitly found through brew prefix
  /opt/homebrew/opt/tomlplusplus/include; verified local extracted copy also
  build/deps/tomlplusplus-3.4.0. No auto downloads; runtime never discovers TOML.
- Important existing fixes: glob ComponentPattern wrapper, discovery splits
  supplied pattern BEFORE joining manifest base (base may contain [!]); native
  alias test keeps .erl lowercase. Decode invokes decode_options; preserve it.
- Plan selected targets before filesystem resolution; definitions/features are
  validated with real PP. No executable/backend generation or stage-reader work.

# Previous parser implementation evidence

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
