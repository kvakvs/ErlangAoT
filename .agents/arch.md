# Architecture

- `docs/compile.md` freezes the LLVM milestone: global stable LLVM 23.1.x (>=23.1.1),
  acyclic small-integer/parameter/direct-call subset and private tagged project ABI v1.
  Private `erlang_codegen` links the SDK through target-local `erlang_llvm_sdk`.
  Global-only CMake discovery validates version/RTTI and host C++ linking; runtime-only
  builds never load LLVM. A move-only private compilation owner retains batch ASTs,
  one context and ordered empty IR modules; owned diagnostics/output buffers survive
  teardown, and errors invalidate staged output. LLVM callbacks retain stable result
  addresses across moves. Explicit target setup retains one machine per batch,
  defaults to host triple/CPU/features, and stamps module triples/data layouts.
  Foreign triples use generic CPUs; missing backends fail without host fallback.
  Only installed X86/ARM/AArch64 backends initialize; PIC/Small are fixed defaults.
  `verify_ir` is the required gate before emission: check current target settings,
  defined functions and whole modules, retaining LLVM failures in project diagnostics.
  Success is never cached across IR mutations; failure clears staged batch outputs.
  `emit_objects` clones verified IR and runs LLVM's legacy machine-code pipeline into
  owned buffers; repeated emission replaces output and failures invalidate the batch.
  Backend printers/parsers support synthetic native and cross-target object tests.
  ABI v1 C++23 headers share namespaced unsigned term/context/function types
  and constexpr version/tag constants;
  constexpr integer codecs check signed 28/60-bit payloads with low tag 0xf.
  LLVM term/signature types derive from the selected target, never host word size.
  Step8 shares stable deferred-feature IDs/names, owner/step/test metadata and an
  escaped context formatter in the ABI headers. Compiler reject_feature latches
  batch failure, clears outputs and marks diagnostics already reported; runtime
  FeatureFailure reports once per operation through a borrowed sink (default stderr)
  and returns fixed-width scoped C++ Status, containing delivery exceptions. No LLVM dependency
  enters runtime reporting. Actual capability/service handlers and CLI integration
  remain later steps; lowering and generated-code execution are still pending.

- Step 9 runtime/context lifecycle is implemented in the LLVM-free static library.
  Runtime startup/create/destroy/shutdown use std::expected, scoped Status and RAII;
  explicit shutdown refuses live contexts, while RAII drains them. C compatibility
  and its lifecycle adapter are removed;
  all APIs are project C++. Generated functions borrow the actual forward-declared
  ProcessContext type using the native machine convention, with no extern-C surface.
  Runtime owns stable contexts with distinct lazy heap/empty mailbox owners and non-recycled
  identities. Context lifetime tokens invalidate before mailbox/heap teardown and
  can survive as dead host bindings; term roots/factories remain deferred.
  One CodeServer outlives contexts and releases registrations before the reserved
  AtomStorage slot. No workers or signals exist. Calls
  require host serialization. `ErlangAoT::generated_program` exports runtime/ABI
  dependencies without LLVM; every generated consumer must use this target.
  `docs/runtime-lifecycle.md` defines ownership, statuses and current boundaries.

- `runtime/include/binary_heap_object.hpp` sketches shared binary objects owning `std::vector<Word>`.
  Refcounted payloads exceed `HEAP_BINARY_THRESHOLD_WORDS` (64 bytes in target words);
  smaller values stay on process heaps and empty refcounted objects are forbidden.
  Immutable word arrays carry optional valid-tail-bit counts; final shared-owner
  destruction releases the vector directly. No binary heap, pool or evacuation service.
  Checked object creation remains an API sketch without an implementation.
- Step 10 adds LLVM-free immediate word services under `runtime/src/terms/` with public
  `erlang_aot/runtime/{base_types,terms}.hpp`: structural immediate classification
  and checked native integer encode/decode sharing ABI v1. Headers/catches and
  noncanonical empty values fail; heap tags return wrong_type without dereferencing.
  Atom/pid/port recognition does not validate runtime IDs. Raw words have no host
  ownership; step 11 adds immediate-only Term values, while factories/roots remain deferred.
  Heap layouts now live privately in `runtime/src/terms/term_layout.hpp`; allocation,
  bignums, graph copying and GC remain reserved. Term/tag/header retain one-word
  representation; Boost bignums honor stronger alignment. No atom table is added.
  Runtime tests cover boundaries, malformed words and all 64 tags; a compiler-side
  fixture checks independently constructed LLVM constants against runtime services.
  Runtime-only builds and the generated-program consumer remain LLVM-free.
- Step 11 implements one runtime-owned CodeServer and one frozen ModuleRegistry per
  module. Generic function/arity/type keys use only all-Term signatures; typed/native
  extensions remain unverified sketches. Publication transfers unique registry ownership;
  ResolvedFunction pins targets and their image, whose destruction follows captures.
  String names await atom binding in step 28; mutation/lookup are host-serialized.
  Immediate-only Term copies need no roots; identities/heap values are rejected.
  Checked calls validate arguments/results, contain host exceptions and report unavailable
  bodies once. abi::v1::dispatch_builtin carries a Status plus success-only output word
  across the native generated-service boundary. Production BIFs, compiler lowering,
  unload and concurrent workers remain deferred. See docs/runtime-builtins.md.
- AtomStorage review API owns runtime-local interning: sequential word-sized atom
  IDs, initially dense ID indexing plus name hash lookup, startup entry cap 2^20
  default / 2^26 hard maximum. Atom GC is a placeholder for reclamation/compaction
  preserving surviving strings/IDs and never recycling IDs; no alternate lookup type.
  Compiler atom constants retain spellings/slots, receive IDs from AtomStorage during
  module initialization, then remain read-only. Bindings/metadata roots are per-runtime
  module instances and retained through pinned code lifetime; no IDs assigned at compile time.
- Runtime process/scheduler review declarations in `runtime/include/{process_heap,
  process,scheduler,mailbox}.hpp` and `processes.md` extend that sketch: one worker per
  logical CPU, owner-thread commands, cooperative tick grants, per-process 1:8:9
  weighted service, sole-live-process idle eligibility and realtime tenure until
  exit (even while blocked). Chunked heap/GC hooks and OS-thread process backend
  are proposals only; no worker, allocator or scheduling behavior is implemented.
  Process owns a FIFO signal inbox; all messages enter it before bounded safe-point
  handling copies payloads into the heap and appends to the receive-only mailbox.
  Signal handling also services waiting/suspended processes without running their code.
  ProcessContext explicitly owns heap/mailbox; heap add and Term::copy_to describe
  rooted graph copies, collect reserves safe-point tracing. Receive cursors preserve
  unmatched messages, remove only a selected candidate and asynchronously park at
  the tail with arrival-version wakeup; process send accepts without waiting for delivery.
  `04-compile.md` references these contracts without expanding its implemented subset.
- Wider code-server proposals reserve exact typed values, explicit generic fallback,
  atom-bound names and concurrent publication. Unverified native templates stay under
  `runtime/include/unverified/`; no conversion registry or virtual call frames exist.
  Cooperative generated-call integration remains deferred.

- Project support lives in `compiler/src/project/`; its private
  toml++ 3.4.0 dependency is discovered locally, with no configure-time downloads.
  Runtime-only builds do not discover TOML. Project-owned command handling exposes
  --project and repeatable --target through thin driver dispatch/option hooks.
  Standalone --new-project writes an annotated default template with C++23 exclusive
  creation, extension completion and failure cleanup; no source tree is required.
- The private project library owns located configuration and bounded TOML loading;
  parsing failures retain manifest coordinates and file I/O accepts native paths.
- Typed project decoding and source discovery retain declaration order, explicit
  path bases, bounded Unicode-aware wildcard matching, and directory symlink policy.
  Per-target source assembly deduplicates native filesystem identities without case folding.
- Pure target selection precedes filesystem resolution. Invocation planning owns
  resolved sources and independent effective frontend options for every selected
  target, validating all work before execution and reserving outputs without writes.
  Execution visits each target/file independently through a shared frontend callback,
  adds diagnostic context and aggregates failures. Default requests preprocess and
  parse, then reach a compile placeholder; successful processing returns 0 without output files.

- CMake fixes project targets to C++23 with warnings as errors, building the host
  tool `erlangaot` and a separate runtime with lifecycle/feature reporting.
  Project validation uses C++23. LLVM SDK linkage is implemented; lowering and
  generated-code/runtime execution remain future work.
  Shared Boost >=1.90 discovery supplies header-only Multiprecision to compiler and
  runtime; runtime consumers inherit its system includes. Root CMake also supplies
  Boost system includes to every project target for orphan-header IDE contexts.
  Matching Homebrew linked headers also get an explicit system path, even with inherited
  global -I flags that CMake inferred as implicit. Parser remains compiler-only;
  runtime-only builds do not require Parser, TOML or OTP. Native compiler tests require OTP >=29.
- SourceManager owns decoded UTF-8/Latin-1 buffers. The incremental Lexer retains
  decoded values, physical spans, logical coordinates and feature-sensitive tokens.
  DirectiveReader handles syntax; PreprocessorSession streams expanded forms,
  diagnostics and immutable feature snapshots. No print/re-lex stage boundary.
- Each preprocessing session owns macros, include/conditional stacks and features.
  Object/arity macros preserve rescan/stringification behavior and expansion traces.
  Includes use explicit paths/application roots plus injectable I/O. Conditions use
  a closed guard evaluator with arbitrary integers, exact term order and bitstrings.
- Shared parsing helpers own bounded token cursors, syntax matching, diagnostics,
  contextual operator metadata and delimiter/fun-prefix tracking. Attribute literal
  normalization reuses private value/comparison/binary helpers; it does not execute
  calls, guards or parse transforms. Grammar families remain separate from PP evaluation.
- ParserSession consumes complete expanded forms transactionally. Ordinary errors
  roll back all arenas/origins, preserve later good forms and latch module failure.
  Token/node/depth/work/diagnostic limits stop resource exhaustion. Syntax errors
  retain expected terminals, invocation/physical traces and unmatched openers.
- ast::Module is move-only, with flat expression/pattern/term/type/form arenas,
  owner/generation-checked IDs, closed variants and const visiting. Owned per-form
  token-origin tables retain half-open extents and EOF anchors after sessions die.
  Per-form features and final/stopping feature state remain distinct.
- The syntax model covers ordinary OTP 29 attributes/records/types/specs and all
  expression, restricted/candidate pattern, guard, control/fun/exception/maybe and
  comprehension families. Literal terms and type syntax use distinct ID categories.
  Qualified/native/inferred records, binary modifiers, overloaded constraints,
  multi-template comprehensions and zipped/strict generators retain their syntax.
- CLI drivers isolate each file, share options/loading/diagnostics and expose
  --preprocess-check, --parse-check, --print-pp and --print-ast. Check modes never
  write executable outputs. Printing uses canonical tokens or an exhaustive
  iterative AST visitor with bounded indentation and an explicit visit budget.
  --verbose traces physical source/include ingestion as [pp] and parser inputs as
  [parse] on stderr; resolved include notifications come from the preprocessor.
- Binding, guard legality, record/type resolution, lint, transforms, lowering and
  execution remain later stages. Stage-reader directories are reserved only.
- Tests combine native invariants/provenance/recovery/stress/mutations, CLI/API
  consumers, offline records and live OTP projections. A pinned grammar reduction
  audit observes all 344 ordinary productions; 79 SSA test productions are excluded.
  Ten checksum-pinned real OTP sources are checked by stage and for deterministic trees.
- Fresh full-build quality requires Lizard CCN <=10 and clang-tidy cognitive <=10
  plus analyzer/bugprone/performance checks, without suppressions or raised limits.
  Host evidence is macOS arm64; Linux x86/ARM and Windows x86-family remain pending.
  Full C++23, compiler-only and ASan/UBSan builds pass all 64 tests; runtime-only
  remains independent. See docs/{projects,project-validation,parser,parser-validation,
  preprocessor}.md for contracts and evidence.
