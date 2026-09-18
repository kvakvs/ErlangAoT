# Architecture

- CMake/C++23 host compiler `erlangaot`, with selectable C++26. Separate placeholder
  runtime archive and ABI interface; no compiler/runtime linkage or LLVM dependency yet.
- `erlang_frontend` owns sources, lexing, directive grammar, and semantic preprocessing.
  Compiler-private Boost 1.90.0 Parser and Multiprecision headers; runtime-only builds
  skip discovery. Character combinators plus a bounded project-token expression cursor.
- `DirectiveReader` is the syntax-only reader; `PreprocessorSession` streams expanded
  forms/file attributes and diagnostics. Source ownership and physical spelling survive
  sessions; logical locations and expansion/include provenance remain separate.
- Macro keys distinguish object and arity forms. Static dependencies and dynamic ancestry
  detect cycles while finite nested arguments remain valid. Raw substitution/stringification
  preserve source order. Include frames own scanners/branches/logical mappings; macros,
  features and prefix state are shared within one module only.
- Conditions use a closed guard AST/value evaluator with arbitrary integers, exact keys,
  Erlang term order, bitstrings and short-circuit semantics after full syntax validation.
  Literal-term parsing is reused for initial definitions and diagnostic directives.
- Pinned OTP 29.1 feature metadata controls incremental keywords and query definitions.
  Includes use injectable filesystem/environment and explicit application directories.
- CLI `--preprocess-check` reports diagnostics without outputs. Compilation remains
  explicitly unavailable. Stage interchange, full parsing/backend/runtime remain deferred.
- CTest combines native semantic/resource/location tests, CLI regressions, offline token
  records, optional exact-version live OTP comparisons, and copied OTP-header smoke tests.
  macOS arm64 is the validated host; Linux/Windows validation remains outstanding.
- Required fresh full-build `check-quality`: Lizard CCN <=10 and clang-tidy cognitive <=10,
  analyzer/bugprone/performance warnings as errors. No threshold relaxation or suppression.
- `references/otp` is an ignored pinned research checkout, not a build dependency.
  See `docs/preprocessor.md`, `.agents/01-pp-otp-tests.md`, and `00-plan.md`.
- Parser plan `.agents/02-parser.md`: pinned grammar/action inventories and raw/epp/lint
  oracle tests. Shared bounded token cursor, syntax/diagnostics, and contextual infix
  metadata now serve directive/condition parsing; typed AST and full parser pending.
