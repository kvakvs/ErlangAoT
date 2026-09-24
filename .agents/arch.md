# Architecture

- `docs/compile.md` freezes the LLVM milestone: global stable LLVM 23.1.x (>=23.1.1),
  acyclic small-integer/parameter/direct-call subset and private tagged C ABI v1.
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
  ABI v1 headers share a C-compatible unsigned term/context/function contract;
  constexpr integer codecs check signed 28/60-bit payloads with low tag 0xf.
  LLVM term/signature types derive from the selected target, never host word size.
  Step8 shares stable deferred-feature IDs/names, owner/step/test metadata and an
  escaped context formatter in the ABI headers. Compiler reject_feature latches
  batch failure, clears outputs and marks diagnostics already reported; runtime
  FeatureFailure reports once per operation through a borrowed sink (default stderr)
  and returns fixed-width C status, containing delivery exceptions. No LLVM dependency
  enters runtime reporting. Actual capability/service handlers and CLI integration
  remain later steps; lowering and generated-code execution are still pending.

- `runtime/include/binary_heap_object.hpp` sketches shared binary objects owning `std::vector<Word>`.
  Refcounted payloads exceed `HEAP_BINARY_THRESHOLD_WORDS` (64 bytes in target words);
  smaller values stay on process heaps and empty refcounted objects are forbidden.
  Immutable word arrays carry optional valid-tail-bit counts; final shared-owner
  destruction releases the vector directly. No binary heap, pool or evacuation service.
  Checked object creation remains an API sketch without an implementation.
- Runtime terms have a sketch in `runtime/include/` with notes in `runtime/design/`: an opaque C++
  `Term`/factory API over private word-aligned heap structs and traceable one-word
  slots and immutable updates. Term/tag/header are one word with explicit low-bit
  masks instead of C++ bitfield/union layout; fixed prefixes reserve trailing storage.
  Boost bignums retain their stronger native alignment; heap/root/GC services remain
  unimplemented. CMake lists headers for IDEs; focused tests compile their assertions.
- Native runtime tests compile `terms.hpp` to check `TermTag::get_kind()` against all
  64 expected tag combinations and private layout assertions; independent ABI tests
  cover integer encoding at both widths. Runtime-only builds remain LLVM-free.
- AtomStorage review API owns runtime-local interning: sequential word-sized atom
  IDs, initially dense ID indexing plus name hash lookup, startup entry cap 2^20
  default / 2^26 hard maximum. Atom GC is a placeholder for reclamation/compaction
  preserving surviving strings/IDs and never recycling IDs; no alternate lookup type.
  Compiler atom constants retain spellings/slots, receive IDs from AtomStorage during
  module initialization, then remain read-only. Bindings/metadata roots are per-runtime
  module instances and retained through pinned code lifetime; no IDs assigned at compile time.
- Runtime process/scheduler review declarations in `runtime/design/{process_heap,
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
- Code-server review sketch uses one unique ModuleRegistry per loaded module,
  frozen at publication. Keys are exact function/arity/argument-type sequences;
  default targets are std::function<CallResult<Term>(ProcessContext&, span<const Term>)>.
  TypedCallable<Args...> passes exact values, including custom types without codecs.
  Generic fallback requires explicit Term arguments; no automatic argument/result
  conversions or conversion registration. ResolvedFunction pins the module for
  checked generic calls; direct pointers/copied typed targets require a retained
  module handle. NativeCallable is only an alias; conversion utilities are deferred.
  Virtual callable/frame preparation is removed; cooperative call ABI remains
  deferred. API sketches only, listed on the runtime target without compilation.

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
  tool `erlangaot` and a separate placeholder runtime.
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
