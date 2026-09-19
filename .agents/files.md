# File map

- `docs/projects.md`: planned project schema, target selection, creation, and path policies.
- `src/project/{CMakeLists.txt,cmake/Dependencies.cmake}`: private pinned TOML
  discovery; `tests/compiler/project/dependency.cpp`: valid/invalid TOML smoke test.
- `src/project/model.hpp`: owned located configuration, per-target options, limits,
  and error records; `tests/compiler/project/{model.cpp,support.hpp}`: model contracts.

Paths below are repository-relative; `src/` in grouped compiler entries means
`compiler/src/`. Public compiler headers live in `compiler/include/erlang_aot/compiler/`.

- `CMakeLists.txt`, `CMakePresets.json`, `Makefile`, `run-macos.sh`: component and
  standard selection, parallel builds, test/format targets, transparent macOS runner.
- `cmake/ProjectOptions.cmake`: language/warnings; `CompilerDependencies.cmake`:
  installed/Homebrew or local Boost discovery; `ErlangDependencies.cmake` and
  `ErlangVersion.escript`: host OTP discovery/version checks.
- `cmake/{CheckComplexity,CheckClangTidy}.cmake`, `QualityToolchain.cmake.in`,
  `.clang-{format,tidy}`, `tools/requirements-quality.txt`: required quality policy.
- `compiler/CMakeLists.txt`: frontend library and executable; `runtime/src/runtime.cpp`
  and `runtime/CMakeLists.txt`: placeholder runtime archive; `abi/CMakeLists.txt`: ABI interface.
- `src/main.cpp`: help/exit contract; `src/driver/options.{hpp,cpp}`: CLI configuration/
  validation; `src/driver/frontend.cpp`: isolated loading, PP/parser, diagnostics/printing.
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
- `.agents/03-project.md`: planned TOML manifest schema, target selection, source
  discovery/options, annotated project creation, and small steps with per-commit gates.
