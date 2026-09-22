# Architecture

- Runtime terms have a manual-review sketch in `runtime/design/`: an opaque C++
  `Term`/factory API over private word-aligned heap structs and traceable one-word
  slots, with immutable updates and a future tagged-value boundary. Declarations
  and layout assertions only, outside CMake; the runtime remains a placeholder.
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
  deferred. API sketches only, outside CMake.

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
  Project validation uses C++23. No LLVM/backend/runtime execution is implemented yet.
  Compiler-private Boost >=1.90 supplies Parser and Multiprecision; runtime-only
  builds do not discover it. Native compiler tests require installed OTP >=29.
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
