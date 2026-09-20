# Current working memory — 2026-09-20

- Default positional/project invocations now preprocess and parse all selected
  sources; successful ASTs reach driver/frontend.cpp's compile_module TODO stub.
  Success exits0 without executable output; explicit checks/printing keep their modes.
  Fresh Debug build, all19 CLI/project CTests, Lizard, affected-production clang-tidy,
  formatting and whitespace pass. Full check-quality not rerun for this uncommitted change.
  Extra tidy on project execution test finds its pre-existing exception-escape in
  main's filesystem path assignment; repository quality gate excludes test sources.
- `--project` now appends `.toml` when the supplied path is missing and lacks
  that suffix; existing paths and filesystem errors retain precedence. Resolver
  lives in project/command.cpp; all18 project CTests pass with new CLI regressions.
  Debug build, full check-quality (Lizard/clang-tidy), format and whitespace pass.
- Completed all22 steps of .agents/03-project.md, each with its own passing-checks
  commit, plus2539c2a for explicit Homebrew discovery. Final documentation/examples
  gate passed all64 Debug tests, fresh full quality, formatting and whitespace.
  Commands, wrapper, links, TOML and canonical template all verified. Native Linux
  x86/ARM and Windows evidence remains pending as recorded in project-validation.
- User steering: C++23 is fixed for all project targets; removed the CMake standard
  selector and enabled COMPILE_WARNING_AS_ERROR. No subagents. DO NOT touch/stage the user-owned
  Makefile clean-target edit. Git writes need require_escalated, git prefix.
- Shell CXXFLAGS=-I/opt/homebrew/include overrides dependency SYSTEM includes and
  exposes third-party warnings under -Werror; use CXXFLAGS= for fresh configuration.
- Fixed-C++23/warnings-as-errors validation: fresh full Debug build, all64 CTests,
  Lizard and clang-tidy passed; all140 translation units use -std=c++23 and -Werror.
- Production support complete: --project, repeatable --target, --new-project;
  all project policy/model/schema/loading/discovery/options/planning/execution/
  creation/build wiring under compiler/src/project. Shared per-file frontend is
  in driver/frontend; generic option/dispatch hooks stay thin. No backend output.
- Homebrew toml++3.4.0 is explicitly discovered via brew prefix and reported at
  /opt/homebrew/opt/tomlplusplus/include. Explicit root wins; version checked;
  no automatic downloads; runtime-only does not discover TOML or frontend deps.
- Final validation: all64 tests pass in full C++23 Debug, compiler-only, ASan/UBSan;
  no sanitizer diagnostics reported. Runtime-only with absent TOML root builds.
  Debug/compiler-only rerun after binary-write byte-limit fixture correction;
  sanitizer already included correction. Step21 full quality/format passes.
- Host macOS26.6.2 arm64, AppleClang21/libc++, CMake4.4.2, Boost1.92,
  OTP29.0.5 /opt/homebrew/opt/erlang/bin/escript. Linux x86/ARM and Windows pending.
  docs/project-validation.md contains exact commands, results and limitations.
- All README/docs frontend shell examples executed successfully. Relative links,
  TOML blocks, help flags and canonical generated template verified. Removed one
  extra documentation-only comment to match actual starter; backend limitation
  remains explicit in surrounding docs. Docs/examples-only step22 has no C++ edits; final gate passed.
- /tmp/project-doc-examples.py runs examples ONCE (creation refuses existing files).
  /tmp/project-doc-checks.py uses .venv-quality/bin/python + pip._vendor.tomli.
  /tmp/erlangaot-project-gate.sh N: fresh full Debug configure/build/CTest parallel2/
  check-quality/whitespace. Never edit while running; never rebuild running tests.
- Formatter /Library/Developer/CommandLineTools/usr/bin/clang-format; quality tools
  in .venv-quality. /tmp/project-focused-tidy.py supplies configured sysroot/includes.
  Lizard CCN10 and clang-tidy cognitive10; no new suppressions/raised thresholds.

# Implementation notes

- Project errors own manifest coordinates, target/key context; decode owns values.
  Select before selected-only filesystem resolution; validate all selected plans
  before processing. Definitions/features use existing PP validation. Mutable
  sessions are never shared across files or targets. Output paths reserved only.
- Discovery splits supplied pattern BEFORE joining manifest base (which may have
  [!]); bounded iterative Unicode matcher uses ComponentPattern strong wrapper.
  Identity dedup handles native aliases; .erl extension is intentionally exact.
- Creation uses NewProjectFilename strong wrapper and C++23 ios::noreplace, detects
  write/close failures and identity-checks best-effort cleanup. No parent creation.
- Frontend stages exchange expanded tokens, not print/re-lex. AST owns flat arenas,
  origins/features and checked IDs; parser rolls back failed forms and latches errors.
  Syntax checks do not perform semantics/lint or parse transforms. Stage readers
  remain reserved under compiler/src/stage_readers/{preprocessed,abstract,ir}.
- Parser validation includes measured344 ordinary OTP29.1 productions (79 SSA
  exclusions), historical AST closure, and ten pinned real OTP sources. Reference
  checkout revision751f87b703fe5948607d08e82599ce644b772e76, gitignored. Bare erl/escript
  may select older asdf OTP28; use the configured Homebrew oracle.
- Preserve parser/preprocessor nuances documented by tests: object macros rescan
  before joining caller; include-return line advances only for immediate LF; native
  records/templates preserve syntax with feature/lint legality deferred. Prefer
  eager cpp_int values over expression-template temporaries in shared exact terms.
