# Step 12 completion — 2026-09-25

- Heap lifecycle moved to runtime/src/memory/heap.cpp; heap_policy.hpp centralizes
  byte-budget validation. allocate takes words, rejects zero/size_t byte overflow
  as invalid_size and budget excess as limit_exceeded; valid requests and collect
  return not_implemented. No backing storage, allocator, safe-point registry or GC.
- memory/copy.cpp implements heap.add and Term::copy_to via checked from_word;
  only smallints/empty tuple/nil, owner independent, may cross runtimes and survive
  both exits. Default invalid slots fail. Roots/graph copying/TermFactory still
  deferred; do not admit heap Terms until external root/lifetime design exists.
- Removed unused contiguous heap/stack/growth sketch fields. Future chunks retain
  addresses; options remain bytes (exact word multiples), usage/capacity words.
  docs/runtime-memory.md specifies host/continuation/mailbox/cursor roots, owned
  pending transit and C++ resource destruction before heap release. Shared binary
  creation stays deferred; corrected stale optional tail sketch to existing zero
  sentinel. No binary allocation implementation or pool.
- Fresh full Debug all90 tests pass; Release runtime-only all15; ASan/UBSan five
  memory/lifecycle/layout tests pass (macOS leak sanitizer disabled; injection
  tracks live allocations). Focused test Lizard/tidy, format and links pass.
  Initial full tidy found a newly trivial heap destructor; defaulted it in the
  header. Final fresh full Debug/all90, full Lizard/tidy, Release all15 and five
  sanitizer checks passed after the fix. Focused production tidy also passed.
- User requested step12 only. Stop before step13; native foreign runs pending.

# Step 11 completion — 2026-09-25

- Fresh Debug all89 CTests + full Lizard/clang-tidy pass. Release runtime-only all14
  and ASan/UBSan4 focused dispatch/rollback tests pass; final trivial-test-copy cleanup
  rebuilt/passed in all3 configurations. Focused test tidy/Lizard, formatting, links
  and whitespace pass. Native Linux/Windows/32-bit and generated Erlang remain pending.
- Generic dispatch now lives in namespaced callable/code_server headers and
  src/builtins/{registry,invocation,bridge}.cpp + src/modules/code_server.cpp.
  Each Runtime owns a CodeServer by value, contexts borrow it; Runtime accessor
  returns null after shutdown. All publication/lookup still host-serialized.
  ModuleDefinition transfers one unique registry; freeze same owner after successful
  map insertion. ResolvedFunction pins module/image; target captures die before image.
- FunctionKey owns name/arity/vector<type_index>, all typeid(Term) for now. add consumes
  Callable&&; explicitly copy lvalue callables. resolve takes named FunctionRequest.
  Typed/native templates remain under runtime/include/unverified/, not implemented.
  String-name metadata avoids fabricating atom storage; atom binding/ABI module
  descriptors deferred to step28. No unload, hot upgrade, workers or production BIFs.
- Term moved to canonical terms.hpp with preserved reserved semantic declarations.
  Only from_word/word/kind/integer_value and default copy/move/dtor work. from_word
  admits smallints and exact empty tuple/list only; no roots/owners needed. Never
  use those trivial copies for future heap values without implementing ownership.
  TermFactory stays top-level sketch; identities/heap values cannot enter calls.
- abi/builtins.hpp dispatch_builtin takes live context, exact borrowed name arrays,
  argument words/arity and output pointer; Status return is separate. Only OK writes
  output, max arity255, null args only arity0. std::function/STL never cross that ABI.
  Checked invocation validates inputs/results, translates exceptions, reports
  unavailable once; CallFailure.reported prevents nested duplicate reports.
  Source compilation/BIF lowering remains unchanged. Stop before step12.

# Step 10 completion — 2026-09-25

- Implemented the raw word boundary only: runtime/include/erlang_aot/runtime/
  {base_types,terms}.hpp + runtime/src/terms/immediate.cpp. classify_immediate checks
  canonical empty encodings, rejects headers/catches as invalid_encoding and heap
  tags as wrong_type without dereference; atom/pid/port classification is structural.
  encode_integer/decode_integer use NativeIntegerEncoding with explicit errors.
  Raw word services have no context/ownership. Term/TermFactory stay declarations
  until external roots/lifetime tracking exist; do not fabricate rooted handles.
  Existing atom-storage/module-root contract unchanged; no separate atom table.
- Private heap layout moved from runtime/include/term_layout.hpp to src/terms/;
  only its layout test has the private include path. Old base_types.hpp forwards
  sketch consumers; old terms.hpp retains host sketch and imports canonical types.
- Fresh Debug automatic SDK build/all86 CTests and full Lizard/clang-tidy passed.
  Focused test-source tidy/Lizard, format dry check, links/whitespace passed.
  Release runtime-only all11 pass; flags/archive have no compiler/LLVM dependency.
  ASan/UBSan runtime tag/layout/immediate all3 pass. LLVM constant agreement fixture
  is test-only and native width; ABI tests separately cover32/64. Native foreign
  runtimes and generated Erlang execution remain pending. Stop before step11.

# LLVM plan progress — 2026-09-25

- User explicitly removed C compatibility after step9. All APIs are C++23 for
  interoperability only inside this project; add C compatibility only if needed later.
  abi/v1.hpp uses namespace types/constexpr constants, Context aliases forward-declared
  runtime::ProcessContext, GeneratedFunction is ordinary native free-function type.
  status.hpp defines enum class Status:uint8_t with preserved numeric values0–10.
  Deleted C .h headers, lifecycle.cpp adapter, extern-C/calling/noexcept macros,
  opaque-handle casts and ProcessContext::abi_handle. Runtime is sole lifecycle API;
  std::expected + unique_ptr own startup, context pointers borrow runtime state.
  RuntimeOptions now owns defaulted ABI version/width checks formerly in C wrapper.
  ABI interface exports cxx_std_23 to standalone consumers. LLVM CallingConv::C
  remains the native machine convention, not a C header/linkage compatibility policy.
  Historical step7–9 C validation below is superseded; future plan/docs use C++ only.
  Fresh full Debug/all84 tests, full Lizard/tidy and focused changed-test quality pass;
  runtime-only all10 and ASan/UBSan5 reporting/lifecycle tests pass. Native archive
  has C++ Runtime symbols and no eaot_v1_* exports. Format/links/whitespace pass.


- Step9 implements LLVM-free Runtime/ProcessContext lifecycle and C ABI runtime.h.
  Runtime owns stable contexts; explicit shutdown BUSY until empty, C++ destructor
  drains survivors. Context pimpl precedes heap/mailbox so lifetime state survives
  mailbox then heap teardown; destructor invalidates host ContextLifetime token first.
  Token is noncopyable, retainable, owner-thread confined; not a GC root registry.
  Lazy heap/mailbox lifecycle only, no allocation/receive/send implementations.
  Runtime identity atomic+local serial never recycle; cap/default1024. C options
  validate version/width and word-multiple heap budgets, default64KiB/64MiB.
  Status values4–10 extend existing0–3, contain construction exceptions, no output.
  Empty shared_ptr CodeServer/AtomStorage reservations: contexts die before code,
  code before atoms. Accessors remain undefined; term/root/scheduler services later.
  ErlangAoT::generated_program interface links one runtime plus ABI/Boost, no LLVM.
  New tests cover owners/repeats/limits/BUSY/RAII/token invalidation and failure sweep
  via isolated new/delete override (including sized delete); no production failhooks.
  Standalone consumer runs through interface, omission fails on lifecycle symbols.
  docs/runtime-lifecycle.md records ownership/serialization/status/link contract.
  Fresh full Debug build/all84 CTests and full Lizard/clang-tidy pass, as do focused
  test quality, runtime-only all10 tests, lifecycle/failure ASan/UBSan, C native link/run,
  six-triple C headers, format/links/whitespace. Step9 complete; stop before10.
  Validation ledger is authoritative.

- Step8 implementation: ABI features.hpp holds23 explicit stable IDs/names plus
  invalid0 sentinel, owners/boundaries/status/plan-step/focused-test metadata.
  feature_diagnostic.hpp formats source/module/target/operation with control-byte
  escaping; unknown IDs are ordinary errors. status.h defines uint32 C failure
  codes with UINT32_C constants (portable C, avoids enum-size ambiguity).
  Compiler reject_feature fails incomplete batch once, clears artifacts, retains
  owned diagnostic context and reported=true delivery flag; default stderr with
  injectable ostream. Runtime FeatureFailure is per-operation, noncopy/move,
  borrows sink, defaults to one fwrite line, catches delivery/format exceptions,
  returns status only; no LLVM/lifecycle/service implementation dependency.
  docs/features.md maps actual/planned extension points. No CLI behavior change;
  actual capability/runtime placeholder placement stays steps17/14.
  Fresh full Debug build/all80 CTests, focused tidy/Lizard, runtime-only all6 tests,
  six-triple C status syntax, runtime reporting ASan/UBSan, formatting/links/whitespace
  passed. Full step8 quality gate passed. Implementation and docs are complete.
  Step8 completed as requested; stop before9.

- Step7 implementation: abi/v1.h exposes uintptr_t term, opaque context, C/cdecl
  function typedef; term.hpp has explicit32/64 checked constexpr integer codecs.
  Low nibble0xf, signed payload28/60; encode unsigned shifts, decode sign without
  signed right shift or overflowing unsigned-to-signed casts. Native uint64_t and
  uintptr_t differ in C++ type on macOS but match size/alignment.
  Term/tag/header one word; tag decoder uses masks, no inactive unions/bitfields.
  Heap sketches now compile fixed prefixes instead of flexible arrays or unsafe
  constructors; Boost cpp_int requires alignment16 here, so BignumCell honors it.
  No term services/rooting/heap allocation implemented. Codegen term_abi derives
  word/signature from target layout and rejects missing/unsupported layouts.
  Focused tests + all75 Debug CTests pass; runtime-only all3 pass, C header syntax
  checks six native/foreign triples, integer ASan/UBSan and focused tidy/Lizard pass.
  Fresh full step7 quality gate passed; formatting/whitespace/local links passed.
  Steps6/7 complete as requested; stop before8.

- Step6 complete: emit_objects reverifies batch each attempt, clones IR, runs legacy
  TargetMachine object pipeline, replaces buffers on retry; errors clear whole batch.
  Backend asm printers/parsers initialized, static components extended. Native LLVM
  tools + object reader architecture/text/symbol checks, ELF/COFF cross emission,
  post-success IR mutation and assembler error pass. Fresh full Debug all72 tests,
  full quality, focused test tidy/Lizard, format/whitespace and static LLVM link pass.
  LLVM23 module inline asm uses Module::GlobalAsmFragment. SDK emits informational
  codegen remarks through callback; tests allow notes, reject warnings/errors.
  User requests steps6 and7; proceed to immediate ABI, stop before step8.

- Step 5 complete: codegen::verify_ir checks current target triple/layout,
  defined functions then whole modules; nonfatal SDK failures become owned errors
  and invalidate all staged batch output. No cached success across IR mutation.
  Emission is still step6 and must invoke verify_ir before producing bytes.
  New IRBuilder tests cover valid/external functions, missing terminator/wrong
  return type, common global with nonzero initializer, target mismatches, repeated
  verification, moves/teardown and failure latching. Fresh full Debug build/all71
  CTests/full quality + focused test tidy/Lizard + format/whitespace passed.
  Focused tidy needs ALL QualityToolchain implicit includes, not just libc++/clang;
  create fixture globals through Module::getOrInsertGlobal to expose SDK ownership.
  User requested step5 only; no lowering/emission/CLI changes. Stop before step6.

- Step 4 complete: explicit codegen::configure_target owns a per-batch TargetMachine
  and stamps module triples/layouts. Empty/matching native triple uses process host
  CPU/features; foreign requests use generic CPU. CMake selects SDK intersection
  X86/ARM/AArch64, initializes those once; PIC/Small fixed, O0/O2 maps None/Default.
  Unknown architecture/unavailable backend errors latch failure without fallback.
  Fresh full Debug all70 CTests + full Lizard/tidy + format/whitespace passed;
  final focused target suite/test-source tidy and static LLVM component link/run
  passed. Native word size/alignment/endianness/features, moves/reuse, foreign
  32/64-bit ELF/COFF layouts checked. No object emission or native foreign execution.
  User requested step4 only; no CLI switches yet. Stop before step5.

- Steps 1–3 completed; user explicitly requested stopping before step4. Step3 adds
  private codegen request/output/result and pimpl Compilation; one independent LLVM
  context per batch, empty IR modules per input, stable callback result storage.
  ASTs/paths owned; modules/context die before result transfer; errors latch and clear
  output buffers. LLVM23 callback takes DiagnosticInfo pointer, not reference.
  Full fresh Debug all69 tests + Lizard/tidy + format/whitespace passed; focused tidy
  includes new tests. ASan/UBSan backend/tests passed with existing frontend/LLVM;
  macOS runtime does not support detect_leaks. No step4 target/IR lowering implemented.

- Step 2 complete: cmake/LLVM{Dependencies,Policy}.cmake restrict discovery to global
  roots, stable23.1.x>=23.1.1, RTTI ON and a real C++23 ABI link probe. C enabled only
  for LLVM dependency probes; imported erlang_llvm_sdk feeds private erlang_codegen.
  All67 tests/full quality gate pass; CLI unchanged. One SDK exists; canonical-path
  selection tested, second installation/native Windows/Linux pending.

- Step 1 complete: docs/compile.md pins global Homebrew LLVM 23.1.1_1, stable 23.1.x
  >=23.1.1. Prefix /opt/homebrew/opt/llvm; real /opt/homebrew/Cellar/llvm/23.1.1_1.
  SDK Release/assertions OFF/RTTI ON/EH OFF, system libc++; X86/ARM/AArch64 available.
  Full fresh Debug gate passed all65 tests and quality. User requests steps1–3 only.
  Each step needs its own [compiler] title commit; clean tree required before starting.

# Current working memory — 2026-09-22

- Binary object API sketch simplified 2026-09-23 at user request: remove BinaryHeap
  and pool design; only binary_heap_object.hpp remains. BinaryHeapObject owns vector<Word>,
  static create(span,tail) returns expected<shared_ptr<BinaryHeapObject>,BinaryHeapObjectError>.
  No owner reference/callback; default destructor frees vector. No pool file ever existed.
  User correction: refcounted word counts must exceed HEAP_BINARY_THRESHOLD_WORDS,
  defined as 64/sizeof(Word) in base_types.hpp; invalid_size rejects smaller/equal/empty.
  Partial valid count 1..wordbits-1; zero low padding;
  checked bit/byte sizes and rollback contract. Creation/accessors remain declarations only.
  term_layout.hpp includes object header and describes vector ownership; other user edits preserved.
  CMake IDE list and architecture/file/compiler-plan notes updated. Strict C++23 consumer
  syntax, focused tidy/Lizard, format and whitespace pass; no commit or full quality gate.

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
