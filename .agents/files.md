# File map

- `run-macos.sh`: build through Make, locate the newest compiler executable in the
  selected build's `bin` tree, and exec with unchanged arguments and caller directory.
- `CMakeLists.txt`, `CMakePresets.json`, `Makefile`: component switches, C++23/26,
  portable configure/build/test presets, macOS/Linux build/test/format/fmt targets.
- `cmake/ProjectOptions.cmake`: target-local language and warning policy.
- `cmake/CompilerDependencies.cmake`: pinned compiler-only Boost.Parser/Multiprecision.
- `cmake/CheckComplexity.cmake`, `CheckClangTidy.cmake`, `QualityToolchain.cmake.in`:
  CCN/cognitive <=10 gates, configured compilation database/includes/SDK.
- `.clang-format`, `.clang-tidy`, `tools/requirements-quality.txt`: formatting policy,
  required checks, pinned local tooling. `.vscode/`: portable CMake IntelliSense setup.
- `compiler/CMakeLists.txt`: frontend static component and `erlangaot` executable.
- `compiler/src/main.cpp`: CLI validation, preprocessing options/check mode, failure codes.
- `compiler/include/erlang_aot/compiler/`: owned source/token/diagnostic/directive/session
  contracts; source/diagnostic/lexer/directive/preprocessor/probe headers.
- `compiler/src/source/source.cpp`: UTF-8/Latin-1 decoding and byte/character positions.
- `compiler/src/diagnostics/diagnostic.cpp`: logical diagnostic rendering and physical traces.
- `compiler/src/lexer/{lexer,numbers,literals}.cpp`: incremental forms, numeric precision,
  string/sigil decoding, feature-sensitive keywords and logical source mapping.
- `compiler/src/parsing/{probe.cpp,boost_parser.hpp}`: standalone Boost boundary/probe.
- `compiler/src/parsing/{token_cursor,token_syntax,operator_info}.*`: bounded lookahead/
  rollback, category-aware syntax, token diagnostics, and contextual infix metadata.
- `tests/compiler/parser/tokens.cpp`: shared cursor/operator/terminal/provenance tests.
- `compiler/include/erlang_aot/compiler/ast/{ids,source,expressions,patterns,forms,module}.hpp`:
  typed IDs, origin ranges, expression/form variants and immutable module access.
  `src/ast/{arena,storage,builder}.*`, `module.cpp`:
  checked flat storage, transactional construction, owned provenance and access.
- `tests/compiler/parser/ast.cpp`: growth/moves, stale/foreign IDs, rollback, exhaustive
  visiting and preprocessing-source lifetime checks, also exercised under sanitizers.
- `compiler/src/preprocessor/preprocessor.cpp`: syntax-only DirectiveReader/recovery.
- `preprocessor/{cursor.hpp,directives.cpp}`: bounded cursor and transactional envelopes.
- `preprocessor/{engine.hpp,session.cpp}`: semantic state, initial definitions, streaming
  effects, error latching and include-frame lifecycle.
- `preprocessor/{macros.hpp,macros.cpp,arguments.cpp}`: definition/overload table, static
  and dynamic cycles, raw arguments, substitution, budgets and object/parameter rescans.
- `preprocessor/{token_utils.hpp,token_utils.cpp}`: generated tokens, canonical stringification.
- `preprocessor/{value.hpp,value.cpp,terms.cpp}`: arbitrary integers, Erlang term order,
  normalized definition tokens and diagnostic terms.
- `preprocessor/{expression.hpp,expression_parse.cpp}`: private AST and bounded token grammar.
- `preprocessor/{expression.cpp,operators.cpp,guards.cpp,bits.cpp}`: guard validation,
  closed BIF/operator dispatch, Erlang evaluation semantics and binary segments.
- `preprocessor/conditions.cpp`: file-local branch transitions and skipped forms.
- `preprocessor/includes.cpp`: injectable host-path/environment/application resolution.
- `preprocessor/builtins.cpp`: module/function macros and logical file mappings.
- `preprocessor/features.cpp`: pinned feature lifecycle/configuration and query definitions.
- `runtime/{CMakeLists.txt,src/runtime.cpp}`, `abi/CMakeLists.txt`: independent placeholders.
- `tests/cli.cmake`: CLI options, module isolation, diagnostics and output preservation.
- `tests/compiler/{probe,lexer,lexer_dump}.cpp`: parser integration/scanner checks and dump.
- `tests/compiler/preprocessor/{forms,semantics}.cpp`: syntax and semantic/resource/location tests.
- `tests/compiler/preprocessor/{oracle,scan,epp_scan}.escript`: OTP 29.1 reference adapters.
- `tests/compiler/preprocessor/*.cmake`, `dump.cpp`: private scanner/semantic comparisons.
- `tests/fixtures/preprocessor/{lexical,semantic}/`: authored cases and pinned token records;
  semantic headers include licensed OTP assert/file smoke fixtures.
- `README.md`, `docs/preprocessor.md`: setup, usage, coverage, limits and host-validation status.
- `regression1.md`: verified OTP 29.1 record-condition compiler crash and upstream report draft.
- `.agents/{arch,files,01-pp,01-pp-otp-tests,plan-windows}.md`: architecture, file inventory,
  preprocessor plan/reference inventory and Windows follow-up. `00-plan.md`: wider project.
- `.agents/02-parser.md`: lexer-reuse/parser/typed-AST implementation plan, six phases,
  18 steps with explicit acceptance and per-step quality/commit requirements.
- `AGENTS.md`: user instructions; `aimemory.md`: working notes; `.gitignore`: local artifacts.
- Ignored `references/otp/`: pinned research checkout, never a build dependency.
- `tests/compiler/parser/{oracle.escript,oracle.cmake,reference.cmake}`: optional
  OTP parser/epp/lint replay and offline scanner/reference-integrity checks.
- `tests/fixtures/parser/`: authored probes, pinned records and production/action inventory.
- `docs/parser.md`: parser implementation progress and validation evidence.
- `compiler/include/erlang_aot/compiler/{features,parser}.hpp`: immutable feature
  snapshots, parser limits/results and streaming/module APIs. `src/parser/parser.cpp`:
  event routing, budgets and recovery; `forms.{hpp,cpp}`, `literals.cpp`, `aggregates.cpp`:
  form dispatch, decoded literals/sigils and bounded recursive aggregate grammar.
- `compiler/src/ast/children.cpp`: exhaustive child ownership validation for expression payloads.
- `compiler/src/parser/{maps,records,structural}.cpp`: map fields, contextual record
  identities and assignments, access/index grammar and restricted postfix chains.
  `tests/compiler/parser/structural.cpp`, `fixtures/parser/phase3/`: structural,
  provenance, resource and OTP parity tests; the parameterized Phase II harness is reused.
- `compiler/src/parser/binaries.cpp`: binary segments, restricted value/size entries,
  ordered modifiers and binary-sigil conversion. `tests/compiler/parser/binaries.cpp`:
  defaults/precision, pattern context, sigil origins, limits and builder invariants.
- `compiler/src/ast/clauses.cpp`: pattern storage, checked function clauses and guard groups;
  `compiler/src/parser/clauses.cpp`: restricted/permissive pattern entries, heads, guard/body
  sequences and function consistency. `tests/compiler/parser/clauses.cpp`: pattern arena
  rollback/ownership, clause invariants, grammar-vs-semantics and source tests.
- `compiler/include/erlang_aot/compiler/ast/operators.hpp`: closed unary/binary identities;
  `compiler/src/parser/expressions.cpp`: bounded Pratt parser, typed calls/remotes and
  nonassociative checks. Shared prefix metadata also serves condition parsing.
- `tests/compiler/parser/expressions.cpp`, `phase2.cmake`, `fixtures/parser/phase2/`:
  structural/provenance/depth checks and native/offline/live OTP Phase II AST/rejection parity.
- `tests/compiler/parser/{forms,dump}.cpp`, `phase1.cmake`: integration/limit/recovery
  tests and private native/OTP AST comparison. `tests/compiler/encoding.hpp`: shared
  scanner/preprocessor/AST test encoding; `fixtures/parser/phase1.*`: include/LF/CRLF probes.
