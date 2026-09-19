# Current working memory — 2026-09-19

- Project checkpoint: steps1–11 committed (latest8f48ca3); step12 applied and full
  gate running session74282. All prior individual full gates passed; step11 all56.
- Remaining scripts /tmp/project-step13.py through step21.py UNAPPLIED in main.
  Scripts12–20 verified in isolated /tmp/erlangaot-project-preview (CLI +17 project
  tests pass); preview21 combined hardening test passes after using SRC/module0.erl
  for native case alias (uppercase .ERL is correctly rejected). Each main step still
  requires its separate fresh full gate/commit. Step22 docs still to prepare.
- Step18 uses NewProjectFilename strong wrapper, C++23 std::ios::noreplace;
  step19 wraps argument accordingly. Preview focused tidy/Lizard pass all new TUs.
- User steering: retain C++23 requirement and remove C++26 checks from project
  validation. Do not run further C++26 checks. Initial ASan/UBSan build compiled
  in build/project-sanitize; rebuild/test final C++23 code for step21 evidence.
- User Makefile clean-target edit must remain unstaged/unchanged. Git commits
  require tool escalation, authorized by plan. Use apply_patch for .agents files.
- Previous checkpoint details below are historical; current checkpoint above wins.

- Latest checkpoint: steps1–7 committed; step6=a1e4b5b, step7=8a739d3. Step8
  implemented, all53 + focused tidy pass; full quality running session76307 with
  /tmp/project-step8-* logs. Glob internal ComponentPattern wrapper fixes genuine
  swappable-parameters finding; preserve it when applying step9 matcher extension.
  Step7 direct/alternative locals changed from const for automatic move per tidy.
- UNAPPLIED scripts now /tmp/project-step9.py through step20.py inclusive; all
  parse. Step16/19 CLI scripts prepared; test actual replacements after formatting.
  Step17 explicitly includes string_view now. Step18 whole-filename suffix fix and
  pure valid_creation_filename helper added; previous pending-review bullets below
  are superseded. Steps21/22 still need actual portability validation/docs.
- User-owned change appeared in Makefile: adds clean target deleting build trees.
  Do not stage, modify, or revert it. User was told active gates use build/debug.

- Active task: execute all22 steps of .agents/03-project.md, separate commits and
  full gates per step; no subagents. Steps1–5 committed 202af94/c1e74c8/c539882/
  d03dd39/2dd5044. Step6 passes all51 + full quality and is ready for its commit.
- Helpers: sh /tmp/erlangaot-project-gate.sh N configures fresh full Debug, builds,
  runs full CTest parallel2 (~72s), then required check-quality (~5min); logs
  /tmp/project-stepN-*. NEVER edit the helper while running. Step3 helper was
  edited while executing; standalone full quality subsequently passed.
- /tmp/project-focused-tidy.py runs specified new TUs with full sysroot/includes.
  Formatter /Library/Developer/CommandLineTools/usr/bin/clang-format.
  Git commits require exec require_escalated (authorized plan). Use apply_patch
  for protected .agents files; shell/Python cannot write them.
- Prepared UNAPPLIED scripts /tmp/project-step7.py through step15.py, plus step17.py
  and step18.py, contain proposed code/tests for subsequent steps. Execute each
  only after preceding step commit; inspect actual changes, format, focused build/
  tests/tidy, fix findings, then full gate. Steps16/19–22 not scripted yet.
  Scripts are preparation, not verified code. Step6's textual insertion missed
  clang-format's blank line; actual decode_options call was fixed with apply_patch.
  Do not rerun applied scripts (would append duplicate CMake/docs content).
- Model is owned Manifest/Target/TargetOptions/Text/Site/Limits/Error; errors throw
  Failure with detail + rendered what(). Loader uses private toml::table and bounded
  native reader. Decode helpers in schema.hpp/cpp. Constant size uses 1'048'576
  after tidy rejected int multiplication. Definitions semantic validation is
  planned via existing PP on empty input in step12, not a second Erlang parser.
- TOML3.4 currently found at /opt/homebrew/include; verified ignored source copy
  build/deps/tomlplusplus-3.4.0 also available. Missing/changed root and runtime-only
  configuration checks pass. Library macros header-only/exceptions/TOML1.0.
- Pending script review: step17 should explicitly include <string_view> in template.cpp.
  Step18 suffix check should use folded whole filename ends_with(".toml"), not
  extension(), to retain a filename exactly ".toml". std::ios::noreplace is supported
  by local C++23 (probe /tmp/project-noreplace-probe.cpp); writer uses injected
  write/close hooks and identity-checked best-effort cleanup, no overwrite.

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
