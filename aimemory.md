# Current working memory — 2026-09-22

- Permanent `tests/runtime/term_tag.cpp` truth-table test covers all64 combinations,
  reports mismatching tags and expected/actual enum values, and remains active under NDEBUG.
  Root registers tests/runtime when native runtime+BUILD_TESTING enabled, independently
  of compiler/OTP. Debug target/CTest and focused Lizard/clang-tidy/format pass.

- TermTag::get_kind() in terms.hpp resolves primary/secondary/tertiary delegation
  using arithmetic lookup into TermKind (base_types.hpp); TermTagKind is retired.
  PID/port resolve to local_pid/local_port; tag3=2/3 to empty_tuple/empty_list.
  TermKind includes header/list/boxed/catch_object/invalid. All64 combinations pass constexpr
  checks. Focused Lizard/tidy,
  strict C++23, formatting and whitespace pass. Existing bitfield layout preserved.

- Bash wrapper failure reproduced with cached CXXFLAGS=-I/opt/homebrew/include:
  Boost.Parser unused parameters failed under -Werror. BoostDependencies now adds
  Homebrew's linked include alias as SYSTEM only if its boost directory resolves
  to the selected Boost tree, and removes that alias from inferred implicit includes
  so CMake actually emits -isystem. Root/Multiprecision interface share these paths.
  This supersedes the cache-clearing workaround below for matching Homebrew installs.
  Validation: original bash wrapper parse-check passes with cached global -I;
  fresh full Debug and runtime-only builds pass, all140 full-build commands retain
  -Werror, all3 CLI/workflow tests and full check-quality pass. Actual-flags probes
  accept Boost headers and reject a project unused parameter. No commit.

- CLion resolved uncompiled term_layout.hpp using erlang_aot (no Boost), although
  erlang_runtime had the correct system path. Root CMake now discovers shared Boost
  when either component is enabled and provides its SYSTEM include to all child
  targets, covering IDE fallback contexts. Target dependency exports remain intact.
  User requests CMake edits, no MCP/computer control for this fix.

- C++23, required standard and disabled extensions now default in root CMakeLists.txt
  for all subdirectories/targets; ProjectOptions.cmake only owns warning policy.

- CLion Boost warnings reproduced despite SYSTEM dependency includes: ~/.zprofile
  exports CXXFLAGS=-I/opt/homebrew/include, shadowing the formula SYSTEM include.
  Cleared cmake-build-debug CMAKE_CXX_FLAGS via reconfigure; no warning-policy change.
  Full CLion build passes. README documents cache cleanup and dependency roots. A focused compile confirms
  Boost is quiet while project unused parameters remain errors. Shell profile is
  unchanged; clearing the CMake cache can reimport CXXFLAGS unless overridden.

- runtime/CMakeLists.txt explicitly lists all11 prototype headers as PRIVATE target
  sources for IDE navigation. They are not separate translation units; runtime.cpp
  remains the only compiled source. Plan/architecture/file map/design notes aligned.
  Preserve user term_layout.hpp edits; header internals remain review sketches.

- Boost build fix: runtime compile flags previously lacked the global Boost include,
  reproduced by cpp_int.hpp probe. cmake/BoostDependencies.cmake now shares discovery,
  version >=1.90 and header-only Multiprecision interface target; CompilerDependencies
  retains Parser-only discovery and links that target. erlang_runtime PUBLICly exports
  runtime/include plus Multiprecision SYSTEM headers. Runtime-only now needs Boost,
  but no Parser/TOML/OTP discovery; older no-Boost runtime validation notes are historical.
  Reconfigured debug; full build, fresh runtime-only build, downstream CMake consumer
  using cpp_int arithmetic, both actual-runtime-flags header probes and4 focused CTests
  pass. Full Lizard plus focused clang-tidy (runtime/probe/lexer numbers) pass. README,
  plan, architecture/files and historical validation note updated. No C++ sketch edits
  or commit; term_layout.hpp is still unfinished and excluded from compilation.

- Keep .agents/04-compile.md synchronized with every runtime/include sketch change.
  Its inventory now links all11 current headers and maps them to numbered steps;
  stale runtime/design header paths are fixed. Steps7/9–13/28 now reflect target
  words/layout review, atom service ownership, signal inbox handling, exact callable
  registries, module freezing/lifetimes and C ABI separation. All46 steps remain
  proposed; conversion utilities and cooperative generated calls remain deferred.
  Documentation-only validation: full header inventory, local links, numbering and
  whitespace checked; no C++ gates needed for this plan update.

- Callable simplification: conversion utilities are removed and deferred at user request.
  User explicitly chose API sketch only. callable.hpp now has Callable (Term span),
  TypedCallable<Args...> (exact values), FunctionKey and a noncopyable ModuleRegistry.
  ModuleDefinition transfers one unique_ptr registry; LoadedModule freezes/owns it.
  Default keys are all-Term, typed signatures infer arity/types without any codec
  requirements. find_typed returns a view of the stored target; no implicit fallback,
  argument decoding or result encoding. Targets return CallResult<Term> explicitly.
  ResolvedFunction pins the module for checked generic calls; direct lookup needs
  callers to retain the module through target destruction. NativeCallable is an
  alias; virtual callable/frame preparation and conversion scaffolding are removed.
  Cooperative
  generated-call ABI remains deferred. Docs/architecture/files/compiler plan aligned.
  Standalone/combined API syntax, custom codec-free types/constraints, clang-tidy,
  Lizard, formatting and whitespace pass. Strict -Wpedantic -Werror still fails only
  on existing terms.hpp nested anonymous unions at127/260. No implementations,
  CMake integration or commit; preserve user's atom_storage.hpp edits.


- Message delivery review update: runtime/include/process.hpp owns a FIFO signal_inbox_
  plus enqueue/handle boundaries; every message (including self-send) goes through it.
  Scheduler routes then services bounded signal batches even for waiting/suspended code.
  Only Process handling appends receiver-owned terms via Mailbox::append_handled_message.
  Diagnostic send replies still await handling, process-facing send only accepts locally.
  Design/architecture/file notes updated. Declarations only, no runtime implementation.
  Focused clang-tidy, Lizard, formatting and whitespace pass. Strict C++23 syntax is
  blocked by existing terms.hpp anonymous nested unions at lines 127 and 260 under
  -Wpedantic -Werror; no suppressions or unrelated layout changes made. No commit.

- User clarification: compiled atoms are read-only constants initialized by runtime
  AtomStorage calls. Compiler emits spellings/slots, never numeric IDs. Explicit
  per-runtime module initialization creates bindings and transfers roots from any
  temporary context into pinned module metadata before publication. Body reads only;
  no per-use interning. Documented in atom_storage/terms/code_server and 04-compile.
- AtomStorage API-only review in runtime/design/atom_storage.{hpp,md}. One per runtime;
  create(context,text) returns atom Term, repeat spelling reuses ID. Sequential IDs
  start0; dense ID indexing and name hash index; no implementations/alternative lookup.
  Startup max_atoms default2^20, hard2^26, inclusive1..hard; cap applies retained entries.
  AtomId=uintptr_t matches private Word; future GC can leave ID gaps and reclaim entries
  without ever changing surviving IDs/spellings or reusing IDs. Numeric exhaustion is
  separate from live cap. Term.atom_id and ProcessContext.atom_storage accessors added.
  collect is declared placeholder; global roots must include metadata/mailbox/transit.
  Native standalone/combined syntax and cap/API assertions, clang-tidy, Lizard,
  formatting/whitespace and local links pass. No runtime implementation or commit.
- Preserve LoadedModule::name_atom() and ExportName arity comments. Module/export
  names are checked atom inputs; published keys own process-independent metadata.
  Export listings root names in the supplied context; resolve accepts strings or Terms.
- Follow-up sketch: ProcessContext explicitly owns ProcessHeap and Mailbox;
  heap.add / Term.copy_to copy rooted graphs; collect declares safe-point GC and
  initial not_implemented result. mailbox.hpp adds begin_receive, cursor.next
  awaitable (forced tail wait), receive removal, saved scan and cancellation contract.
  ProcessContext.send accepts without waiting; SchedulerPool.send returns delivery reply.
  04-compile now links the sketches and references ownership/roots/wait boundaries.
  User-added TickBudget comment (one unit roughly a function call) preserved.
  Combined/standalone syntax and temporary async API consumer, focused clang-tidy,
  Lizard, formatting, whitespace and local documentation links verified. Still review-only.
- Process/scheduler review-only skeleton added in runtime/design/{process_heap,
  process,scheduler}.hpp and processes.md. Matches term sketch scope, no CMake
  integration or execution yet. Per logical CPU, cooperative resume/budget boundary,
  owner-worker command futures, per-process weighted 1:8:9 dispatch frequency.
  Literal idle counts all live assigned processes; realtime reservation persists
  through wait/suspend/priority changes until exit. Those interpretations need review.
  Chunked stable-address growth with GC placeholder; owned cross-process signals;
  OS-thread process backend only reserved. No compile-plan steps completed.
  Combined/standalone native C++23 syntax, focused clang-tidy, Lizard, formatting
  and whitespace checks pass. No behavior tests, full-build gate or commit.
- User directs that 04-compile must use/build upon the term library sketch.
  Plan now makes it the foundation, with explicit links and ABI/lifecycle/term/
  memory step requirements; open design details remain refinable during implementation.
- Runtime term API/layout sketch added under runtime/design for manual review only;
  no executable implementation/build integration. User specifically wants private
  structs with controlled heap memory layout as well as opaque convenient term API.
  terms.hpp declares Term/TermFactory; term_layout.hpp sketches word-sized slots,
  heap prefixes and assertions; terms.md covers immutable updates, ownership/errors,
  GC scanning and future tagging. 04-compile links it; no plan step is completed.
  Native C++23 syntax/layout, focused clang-tidy, Lizard, format/whitespace pass;
  no 32-bit validation, full-build gate or commit for this review-only draft.
- `--verbose` traces [pp] source/resolved include paths and [parse] original module
  paths on stderr for positional and project modes. Parser consumes a stream, so
  its start trace precedes include traces. PreprocessorOptions::include_loaded
  observes successful include decoding; no inactive/candidate/logical-file traces.
  Build, all26 selected CLI/project/PP/parser tests (CLI assertion corrected then
  rerun), Lizard, affected-production clang-tidy, formatting and whitespace pass.
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
