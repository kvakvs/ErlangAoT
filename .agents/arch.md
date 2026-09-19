# Architecture

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
  adds diagnostic context and aggregates failures; unsupported compilation writes nothing.

- CMake builds the C++23 host tool `erlangaot` and a separate placeholder runtime.
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
