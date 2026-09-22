# Current working memory — 2026-09-22

- Callable simplification supersedes the older native-dispatch/frame notes below.
  User explicitly chose API sketch only. callable.hpp now has Callable (Term span),
  TypedCallable<Args...> (exact values), FunctionKey and a noncopyable ModuleRegistry.
  ModuleDefinition transfers one unique_ptr registry; LoadedModule freezes/owns it.
  Default keys are all-Term, typed signatures infer arity/types without any codec
  requirements. find_typed returns a view of the stored target; no implicit fallback,
  argument decoding or result encoding. Targets return CallResult<Term> explicitly.
  ResolvedFunction pins the module for checked generic calls; direct lookup needs
  callers to retain the module through target destruction. NativeCallable is an
  alias, NativeArguments/virtual Callable/CallFrame preparation removed; optional
  codec declarations and ConversionLimits stay in native_types.hpp. Cooperative
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
- User correction: never automatically convert arguments on call. Each MFA now holds
  exact native type signatures and optional all-Term fallback. Boxed prepare only selects
  all-Term; prepare_native matches NativeArguments exact C++ types, else requires a generic
  registration AND explicit caller-supplied Term arguments. No probing decoder, implicit
  boxing/unboxing, widening, container adaptation, partial wildcard or result-type dispatch.
  NativeCodec decode remains explicit utility; return encoding unchanged. Preserve user
  LoadedModule::name_atom() addition and ExportName arity comment.
  Updated headers/template and dispatch-usage consumers, exact documentation example,
  clang-tidy, Lizard, format/whitespace and local links pass; still no runtime behavior.
- CodeServer review sketch in runtime/design/code_server.{hpp,md}, callable.hpp,
  native_callable.hpp and native_types.hpp. Immutable module publication; exact MFA;
  unload removes map entry but resolutions/frames pin CodeImage. NativeCallable<R(Args...)>
  has fixed arity, optional injected ProcessContext, shared std::function binding;
  checked scalar/Term codecs, UTF-8 binary strings, u32 character lists, recursive
  owning iterable output and ordered append/array input. CallFrames bridge scheduler
  suspension; native bodies stay bounded/synchronous. ProcessContext exposes code_server().
  04-compile links review only; no CMake integration, executable services or plan completion.
  User changed CodeServer module/export name fields from strings to Term during work;
  preserved as checked atom inputs. Registry extracts process-independent keys; export
  listings take ProcessContext to root output atoms; resolve has string and Term overloads.
  Native standalone/combined syntax, positive/negative template constraints, registration
  consumer, clang-tidy, Lizard, formatting/whitespace/local links pass. No runtime tests/commit.
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
