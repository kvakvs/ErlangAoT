# Architecture

- Public CLI object emission is exercised by a separately configured Clang/runtime
  consumer at O0/O2. It registers both modules, validates ABI rejection, runs
  decoded integer calls and immediate identity boundaries, and explicitly tears down.
  Native linking remains test-owned; the consumer must never acquire the LLVM SDK.

- `--impldebug` carries a value-owned set of signed decimal step IDs through
  positional/project frontend requests and private backend requests. Step-specific
  diagnostics query `enabled(step)` independently of ordinary verbosity.

- Local implementation inference owns a separate bounded type graph and AST side
  tables. Unknown inputs remain top; identity/projection results retain argument
  positions independently of specifications. Traversal is iterative and budgeted.
  Resolved dependency order instantiates each call's relations afresh. Conservative
  singleton/contract comparison warns without guards, narrowing or representation
  assumptions; `--impldebug 23` through `27` report escaped implementation summaries.

- Windows executables reserve 8 MiB stacks for bounded recursive parsing. Lexer
  state and preprocessor application overrides use vector storage with allocation-free
  moves; application lookup reads last matching override. CLI dispatch is separate
  from the failure-contained process entry point. Exact binary64 integer conversion
  uses a bounded native mantissa and arbitrary-precision shifts.

- OTP source validation follows official `maint-29`, with an exact reviewed commit
  in `references/otp-pin.cmake`; `pinned.cmake` consumes it without network access.
  Refresh before future OTP-dependent work and review hashes/grammar evidence;
  `docs/otp-reference.md` defines the workflow. Historical records retain provenance.

- LLVM's compression lookups prefer installed libraries. Windows builds missing
  zlib/zstd download pinned 1.3.2/1.5.7 and build static Debug/Release libraries during configuration,
  retaining compiler/architecture/CRT-specific installations under `thirdparty/`.
  MSVC lookups exclude MinGW archives and clear incompatible cached selections.
  Linux/macOS keep system discovery; runtime-only builds never request these libraries.

- Windows configuration requires installed Clang before language detection. Default
  Boost 1.90.0 and toml++ 3.4.0 sources/archives persist in ignored `thirdparty/`, with
  pinned SHA-256 downloads, serialized extraction and offline reuse across build trees.
  Explicit roots take precedence; LLVM retains its global SDK/version/link policy.
  Native Windows hosts require MSVC ABI plus a linkable Windows SDK/C++23 library;
  clang-cl/Ninja Multi-Config presets select Debug/Release, with DLL CRT defaults
  (/MDd, /MD) and UTF-8 MSVC source flags. Nested consumer/SDK tests inherit the
  parent generator, compiler, architecture, configuration and CRT. Runtime remains
  static; the Windows x64 Debug compiler/runtime gate uses the pinned LLVM 23.1.2 SDK.

- `docs/compile.md` freezes the LLVM milestone: global stable LLVM 23.1.x (>=23.1.1),
  acyclic small-integer/parameter/direct-call subset and private tagged project ABI v1.
  Private `erlang_codegen` links the SDK through target-local `erlang_llvm_sdk`.
  Global SDK discovery or pinned fallback validates version/RTTI and host C++ linking; runtime-only
  builds never load LLVM. A move-only private compilation owner retains batch ASTs,
  one context and ordered IR modules; owned diagnostics/output buffers survive
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
  enters runtime reporting. Compiler/runtime capability handlers are implemented;
  explicit CLI artifacts and Windows x64 native harness execution are implemented.

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
  Immutable word arrays carry valid-tail-bit counts (zero means full words); final shared-owner
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
  Owned string metadata awaits future runtime atom initialization; mutation/lookup are host-serialized.
  Immediate-only Term copies need no roots; identities/heap values are rejected.
  Checked calls validate arguments/results, contain host exceptions and report unavailable
  bodies once. abi::v1::dispatch_builtin carries a Status plus success-only output word
  across the native generated-service boundary. Production BIFs, compiler BIF lowering,
  unload and concurrent workers remain deferred. See docs/runtime-builtins.md.
- Step 12 places heap lifecycle, byte-budget policy and memory boundaries under
  `runtime/src/memory/`. Word requests reject zero/byte overflow and budget excess;
  valid allocation and collection return not_implemented, accounting stays zero.
  Heap add/Term::copy_to revalidate owner-independent immediates without allocation;
  copies survive source/destination exit and may cross runtimes. No heap Terms,
  graph copies, roots, binary allocation or collector are enabled. Future roots
  cover host/continuation/mailbox/cursor state; signals own independent transit data.
  C++ cell resources require destruction, never byte relocation of shared handles.
  `docs/runtime-memory.md` defines units, errors, teardown and shared binary contracts.
- Step 13 adds runtime-owned SchedulerService lifecycle bookkeeping, separate from
  proposed Scheduler/Pool workers. Existing contexts register explicitly once;
  records hold identity/state/suspension/terminal reason, never heap/context pointers.
  begin/finish_dispatch record transitions only; no code, queue or budget runs.
  Resume cannot wake waiting state; wake/signal/receive integration remains reserved.
  remove retires a non-running identity; context destruction removes its record or
  returns busy while running. Registration allocation failure leaves retry possible.
  Shutdown closes new work but permits returns/inspection/removal. Runtime RAII
  clears records, destroys contexts, releases code, then destroys the stopped service.
  Host serialization remains mandatory. See docs/runtime-scheduler.md.
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
  parse, validate, infer, lower, optimize and emit in memory; explicit emission publishes
  only after every selected target succeeds. Production executable linking remains deferred.

- CMake fixes project targets to C++23 with warnings as errors, building the host
  tool `erlangaot` and a separate runtime with lifecycle/feature reporting.
  Project validation uses C++23. LLVM SDK linkage and private generic lowering are
  implemented, with generated-code/runtime execution checked by separate native consumers.
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
  iterative AST visitor with parenthesized objects, two-space indentation and
  name=value scalar/child fields. Indentation stays bounded after depth 64; an
  explicit visit budget bounds traversal. There is no AST text reader.
  --verbose traces physical source/include ingestion as [pp] and parser inputs as
  [parse] on stderr; resolved include notifications come from the preprocessor.
- General guard legality, lint/transforms and execution remain later stages. The
  supported subset has binding/call/type analysis, inference and private generic lowering;
  stage-reader directories are reserved only.
- Tests prefer real source/project CLI workflows, exact AST/diagnostic snapshots,
  bounded source stress/mutations and separately built frontend/runtime consumers.
  Keep API-only invariants, raw-stage ownership, injected limits/faults and cross-width
  ABI boundaries. Backend synthetic cases retire only after real compiled Erlang
  covers them; see docs/test-migration.md and .agents/04-compile.md steps 15–46.
  Existing offline records and live OTP projections remain. A pinned grammar reduction
  audit observes all 344 ordinary productions; 79 SSA test productions are excluded.
  Ten checksum-pinned real OTP sources are checked by stage and for deterministic trees.
- Fresh full-build quality requires Lizard CCN <=10 and clang-tidy cognitive <=10
  plus analyzer/bugprone/performance checks, without suppressions or raised limits.
  Historical macOS arm64 evidence includes all 64 then-current tests in full,
  compiler-only and ASan/UBSan builds. Windows x64 migration evidence is recorded
  in docs/test-migration.md, including historical parser/quality failures and the remaining frontend-ASan
  limitation; runtime-only remains independent and passes all 15 tests under ASan.
  Linux x86/ARM and native 32-bit runs remain pending. See docs/{projects,project-validation,parser,parser-validation,
  preprocessor}.md for contracts and evidence.

- Step 14 installs host reporting placeholders for TermFactory, heap allocation/GC,
  send, SchedulerService run/execute and CodeServer unload. Typed diagnostic_failure
  preserves delivery failure; state and owners remain unchanged. Factory retains only
  a weak context token and borrowed sink. BIF bridge resolves registrations first,
  then recognizes a bounded exact deferred signature catalog; unknown_builtin=11
  distinguishes other missing signatures. No atoms, workers, roots or loader ABI are
  invented; atom collection stays reserved. See docs/runtime-services.md.

- Compiler steps 15–21: the driver owns complete positional/target AST batches.
  LLVM-free side tables validate module/function/export identities, subset capabilities,
  parameter positions and exact local/remote calls. Each project target is isolated;
  iterative dependency ordering rejects executable cycles. Versioned symbols and
  type-variable scopes encode names without delimiter collisions.
- `semantic/types/` interns bounded symbolic types, flattens joins and widens to top
  on exhausted limits. Declared aliases, record contracts, overloads and constraints
  borrow source provenance. Recursive references remain finite; memoized substitution
  respects opaque module boundaries and preserves nominal identities. Independent inference
  feeds generic lowering; type inspection reports both domains before entering LLVM.
- Final step 21 Windows x64 Debug validation: 78/78 CTests and full Lizard/clang-tidy
  pass. Earlier migration failures remain historical; full frontend sanitizers and
  additional native platforms still require validation.

- Steps 24–27 lower validated batch declarations, integer/parameter returns and
  resolved calls into separately verified LLVM modules. Every entry retains the
  generic target-word ABI; specifications never create representation assumptions.
  Explicit iterative traversal preserves source-order argument evaluation and the
  process context. Exported remote identities become matching external declarations.
  Real-source adapter tests inspect native/cross-width objects and ABI data flow;
  step 28 adds registration and native harness execution; steps 35/36 add CLI publication.
  Final Windows x64 Debug 80/80 and full Lizard/clang-tidy pass. Historical step 27 validation.

- Step 28: target-layout descriptors and retained registration entries call a native
  C++ runtime service. Publication copies names, validates ABI/width/exports and
  freezes a unique registry; resolved handles pin code images. Atoms remain reserved.
  Separately linked real-source objects execute through the mandatory runtime.

- Step 29: speed-only specialization planning consumes bounded implementation profiles,
  recognizes exact entry-block small-integer checks and deduplicates useful constraints.
  Caps are 3/function, 32/module, 128/target and 2x generic IR including dispatch.
  Current supported source has no removable checks and therefore stays generic.

- Step 30: LLVM clones remove recognized checks only under runtime low-tag guards.
  Transactional dispatch installation preserves public symbols, descriptor references
  and the original generic fallback. Actual clone/dispatch instruction counts enforce
  2x function/module limits before publication; no frontend language expansion.

- Step 31: target-aware PassBuilder selects LLVM standard O0/O2 pipelines, with
  full batch verification on both sides and per-module analysis lifetimes.

- Step 32: verified assembly snapshots and text/bitcode output own their bytes;
  serialization uses LLVM writers without adding product readers.

- Step 33: the LLVM-free artifact publisher validates encoded module paths and
  physical aliases, stages complete batches with checked exclusive writes, then
  uses platform file replacement. Publication is not atomic across multiple files.

- Step 34: driver backend options retain explicit optimization/emission policy
  independently of preprocessing and project selectors; conflicts fail before I/O.

- Step 35: positional batches retain ASTs through LLVM-free semantic analysis and
  the shared backend, then optimize and emit in memory. Explicit emission publishes
  only after all source modules succeed; ordinary LLVM remarks remain opt-in.

- Step 36: project execution passes full target/invocation context to the shared
  frontend/backend. Each target owns an independent batch and encoded artifact root;
  complete serialized results wait in a driver queue until all selected targets succeed.

- Step 37: an optional synchronous backend observer reports only started phases.
  Driver rendering escapes user names and writes [comp] events to stderr; bounded
  specialization profiles retain concrete policy/benefit/budget reasons.

- Step 38: IR inspection reuses lowering/specialization and verified LLVM text
  serialization, retaining a before snapshot only when requested. Optimized inspection
  runs the selected standard pipeline; neither path emits objects or publishes files.

- Step 39: type inspection stops after shared semantic analysis, before LLVM state.
  Source-ordered modules/functions/expressions distinguish declared contracts from
  inferred facts, unknown inputs and argument relations. Recursive aliases stay
  symbolic; bounded displays and graph widening are explicit.
