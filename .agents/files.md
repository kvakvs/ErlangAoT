# File map

Paths are repository-relative; `src/` in compiler entries means `compiler/src/`.
Public headers live in `compiler/include/erlang_aot/compiler/`.

- `CMakeLists.txt`, `CMakePresets.json`, `Makefile`, `run-macos.sh`: component and
  configuration, parallel builds, test/format targets, transparent macOS runner.
- `cmake/ProjectOptions.cmake`: fixed C++23 and warnings as errors; `CompilerDependencies.cmake`:
  installed/Homebrew or local Boost discovery; `ErlangDependencies.cmake` and
  `ErlangVersion.escript`: host OTP discovery/version checks.
- `cmake/{CheckComplexity,CheckClangTidy}.cmake`, `QualityToolchain.cmake.in`,
  `.clang-{format,tidy}`, `tools/requirements-quality.txt`: required quality policy.
- `compiler/CMakeLists.txt`: frontend library and executable; `runtime/src/runtime.cpp`
  and `runtime/CMakeLists.txt`: placeholder runtime archive; `abi/CMakeLists.txt`: ABI interface.
- `src/main.cpp`: help/exit contract; `src/driver/options.{hpp,cpp}`: CLI configuration/
  validation; `src/driver/frontend.{hpp,cpp}`: shared per-file loading, PP/parser,
  diagnostic callback and printing, with a positional-mode adapter.
- Public `{source,token,diagnostic,directive,preprocessor,features,parser,printing}.hpp`:
  source/token/events, immutable features, parser ownership/limits/results and output APIs.
- `src/source/source.cpp`: decoding/positions; `src/diagnostics/diagnostic.cpp`: logical
  locations, physical traces and opener rendering.
- `src/lexer/{lexer,numbers,literals}.cpp`: incremental scanning, arbitrary numeric
  values, strings/sigils and keyword state.
- `src/parsing/{probe.cpp,boost_parser.hpp}`: Boost boundary;
  `{token_cursor,token_syntax,operator_info,delimiters}.{hpp,cpp}`: shared token mechanics.
- `src/preprocessor/preprocessor.cpp`: DirectiveReader and form recovery;
  `directives.cpp`, `cursor.hpp`: directive envelopes; `engine.hpp`, `session.cpp`:
  semantic session; `conditions.cpp`, `includes.cpp`, `features.cpp`, `builtins.cpp`:
  conditionals, include frames, features and contextual definitions.
- `src/preprocessor/{macros,arguments,token_utils}.cpp`: macro expansion/arguments;
  `{expression_parse,expression,operators,guards}.cpp`: closed condition grammar/evaluation;
  `{terms,value,bits}.cpp`: shared literal terms, exact operations and binary encoding.
- Public `ast/{ids,source,module}.hpp`: checked IDs, owned origins and move-only owner;
  `ast/{expressions,patterns,clauses,forms,terms,types,operators}.hpp`: closed syntax payloads.
- `src/ast/{arena,storage,builder}.hpp`, `{builder,module}.cpp`: flat arenas, transactions,
  source ownership and immutable access; `children.{hpp,cpp}`, `clauses.cpp`,
  `control.cpp`, `exceptions.cpp`, `comprehensions.cpp`, `attributes.cpp`,
  `types.cpp`, `specifications.cpp`: exhaustive child/category/shape invariants.
- `src/parser/parser.cpp`: module event routing/budgets/recovery; `forms.{hpp,cpp}`:
  form dispatch; `diagnostics.cpp`: work accounting, expected tokens and opener origins.
- `src/parser/{literals,aggregates,expressions}.cpp`: decoded literals, containers, Pratt
  expressions/calls; `clauses.cpp`: function/pattern/guard sequences;
  `{maps,records,structural,binaries}.cpp`: structural postfix and binary grammar.
- `src/parser/{control,funs,exceptions,comprehensions}.cpp`: blocks/branches/receive,
  funs/references, try/maybe, templates and qualifier groups.
- `src/parser/{attributes,declarations,documentation}.cpp`: attribute shapes,
  records/defaults and documentation metadata; `attribute_values.hpp`: group/list helpers;
  `{attribute_terms,term_value,term_bits}.cpp`: bounded literal normalization.
- `src/parser/{types,type_primary,type_structural,type_names}.cpp`: type precedence,
  aggregates/funs and builtin classification; `specifications.cpp`: overloads/constraints.
- `src/printing/{source,token_text}.cpp`: source output and shared canonical tokens;
  `printable.{hpp,cpp}`: shared Erlang Unicode/control character decoding for AST and term printers;
  `tree.{hpp,cpp}`, `tree_{forms,expressions,structural,control,exceptions,comprehensions,
  attributes,types,specifications}.cpp`: iterative typed AST output with escaped strings
  for nonempty proper lists of printable character integers.
- `tests/cli.cmake`: CLI contracts; `tests/compiler/{lexer,printing,printing_ast}.cpp`:
  scanner/printing; `tests/compiler/preprocessor/`: PP native and OTP oracle tests.
- `tests/compiler/parser/{tokens,ast,forms,expressions,clauses,structural,binaries,
  control,exceptions,comprehensions,attributes,types,specifications}.cpp`: grammar,
  ownership, provenance and invariant tests; `hardening.cpp`, `mutations.cpp`,
  `consumer.cpp`: stress, generated recovery/determinism and post-session API use.
- `tests/compiler/parser/{dump.cpp,operators.hpp,terms_dump.hpp,types_dump.hpp}`,
  `oracle.escript`: exhaustive native/OTP structural projections; `tests/compiler/encoding.hpp`:
  shared exact encodings; `{reference,oracle,phase1,phase2}.cmake`: existing suites.
- `tests/compiler/parser/historical.cmake`: seed AST closure and offline inventory audit;
  `coverage.{cmake,escript}`: measured pinned reductions; `pinned.cmake`: source verification;
  `corpus.cmake`: separate real-source preprocessing/parsing/determinism checks.
- `tests/fixtures/parser/`: immutable original records and phase-specific probes;
  `phase5/coverage.tsv`: attribute/type row index; `phase6/`: measured full inventory,
  closure fixtures, historical ASTs and checksum-pinned real OTP source manifest.
- `docs/{preprocessor,parser,parser-validation}.md`: contracts and evidence;
  `.agents/02-parser.md`: ordered plan/status; `references/otp`: ignored research checkout.
  `compiler/src/stage_readers/{preprocessed,abstract,ir}/` remains reserved only.

- `src/project/{CMakeLists.txt,cmake/Dependencies.cmake}`: private toml++ 3.4.0,
  explicit-root precedence and Homebrew formula-prefix discovery.
- `src/project/model.hpp`: owned located configuration, target options, limits and errors;
  `{loader,diagnostics}.{hpp,cpp}`: bounded native TOML reading and located failures.
- `src/project/{decode,schema,decode_options}.{hpp,cpp}`: strict versioned schema,
  typed target/options decoding, unknown-key checks and configuration budgets.
- `src/project/paths.{hpp,cpp}`: native UTF-8 paths, explicit bases and literal fallback;
  `{glob,glob_utf8}.{hpp,cpp}`: iterative bounded Unicode wildcard matching.
- `src/project/discovery.{hpp,cpp}`: sorted bounded traversal and symlink policy;
  `{sources,identity}.{hpp,cpp}`: ordered source assembly and physical deduplication.
- `src/project/selection.{hpp,cpp}`: ordered target selection and selector diagnostics;
  `options.{hpp,cpp}`: independent effective settings and real frontend validation.
- `src/project/plan.{hpp,cpp}`: selected-target preflight and output collision checks;
  `execution.{hpp,cpp}`: ordered callbacks, diagnostic context and failure aggregation.
- `src/project/{cli,command}.{hpp,cpp}`: project operands, conflicts, help, dispatch
  and `.toml` completion for missing manifest paths;
  `template.{hpp,cpp}`: annotated defaults; `create.{hpp,cpp}`: exclusive native creation,
  extension completion and identity-checked write/close failure cleanup.
- `tests/compiler/project/`: matching unit tests, dependency/model/support contracts
  and local CMake ownership; `cli.cmake`: public invocation and creation contracts;
  `workflow.cmake` + `tests/fixtures/project/workflow/`: multi-target/include/search,
  deterministic output, native-path and no-write regressions; `hardening.cpp`: combined
  resource limits and filesystem capability/alias checks.
- `docs/projects.md`: delivered format, precedence, discovery and creation workflows;
  `docs/project-validation.md`: C++23 evidence and pending host matrix;
  `examples/project/{project.toml,src/main.erl}`: runnable two-target frontend example.
- `.agents/03-project.md`: implementation plan, per-step validation ledger and status.
