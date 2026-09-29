# File map

- `compiler/src/implementation_debug.hpp`: sorted unique step selection/query API.
  `compiler/src/driver/implementation_debug.{hpp,cpp}`: checked integer/list CLI
  parsing with atomic merging of repeatable `--impldebug` operands.

- `compiler/src/semantic/types/inference.{hpp,cpp}`: independent implementation
  facts, parameter/result relations and bounded iterative expression analysis.
  `tests/compiler/semantic/inference.cpp`: temporary relational/budget invariants.
- `compiler/src/semantic/types/{contracts,membership,trace}.cpp`: conservative
  singleton/spec warnings, bounded exact membership and step 23 debug summaries.

- `references/otp-pin.cmake`: current maint-29 source revision and branch.
  `docs/otp-reference.md`: refresh procedure, checksum/grammar review and validation.
  `tests/compiler/parser/pinned.cmake`: shared offline revision/cleanliness/hash gate.

- `docs/compile.md`: pinned SDK provenance/tools, frozen compilation subset, provisional
  ABI and command/artifact contract; `.agents/04-compile.md`: ordered implementation plan.

Paths are repository-relative; `src/` in compiler entries means `compiler/src/`.
Public headers live in `compiler/include/erlang_aot/compiler/`.

- `CMakeLists.txt`, `CMakePresets.json`, `Makefile`, `run-macos.sh`: component and
  configuration (root CMake requires C++23 without extensions in all subdirectories
  and supplies shared Boost system includes for IDE header analysis),
  parallel builds, test/format targets, transparent macOS runner.
- `make-{build,test,format,clean}.bat`: Windows equivalents of the Makefile targets;
  `erlangaot.bat`: build then run the selected configuration with caller-relative arguments.
- `cmake/ProjectOptions.cmake`: target warnings as errors; `BoostDependencies.cmake`:
  shared installed/Homebrew/local Boost discovery and Multiprecision interface target,
  including SYSTEM classification of Homebrew's matching linked include alias;
  `CompilerDependencies.cmake`: compiler-only Parser discovery; `ErlangDependencies.cmake` and
  `ErlangVersion.escript`: host OTP discovery/version checks.
- `cmake/WindowsToolchain.cmake`: fail early without a runnable installed Windows Clang;
  `WindowsHost.cmake`, `probes/windows.cpp`: native MSVC ABI/SDK/C++23 checks and
  default DLL CRT; `CMakePresets.json`: clang-cl/Ninja Multi-Config Windows presets;
  `TestHost.cmake.in`: parent toolchain/CRT/dependency settings for nested native tests;
  `ThirdPartyDependencies.cmake`: SHA-256-verified archives/extraction retained under
  ignored `thirdparty/`, shared by Windows Boost and toml++ dependency discovery.
- `cmake/{CheckComplexity,CheckClangTidy}.cmake`, `QualityToolchain.cmake.in`,
  `.clang-{format,tidy}`, `tools/requirements-quality.txt`: required quality policy.
- `cmake/{Zlib,Zstd}Dependencies.cmake`, `modules/Find{ZLIB,zstd}.cmake`,
  `WindowsDependencyBuild.cmake`: LLVM-scoped installed compression library detection
  and shared Windows pinned download/static build fallback; retained under
  `thirdparty/` with compiler/architecture/CRT-specific Debug and Release libraries.
- `cmake/LLVMDependencies.cmake`, `LLVMPolicy.cmake`, `probes/llvm.cpp`: global-only
  LLVM 23.1.x discovery, path/version policy, host ABI link probe and available
  X86/ARM/AArch64 backend selection/component linkage.
- `compiler/src/codegen/sdk.{hpp,cpp}`: private SDK version boundary;
  `tests/compiler/codegen/{dependency.cmake,CMakeLists.txt}`: independent SDK
  consumer build/run, discovery/rejection fixtures and LLVM-independent runtime build.
- `compiler/src/codegen/{request,output,result}.hpp`, `result.cpp`: move-only batch
  requests/results, owned diagnostics/bytes and latched failure/completion status.
  `compilation.{hpp,cpp}` owns stable context/module state behind a private interface;
  `llvm_state.hpp` confines LLVM access; `diagnostics.cpp` copies SDK callbacks safely.
  `tests/compiler/codegen/{results,ownership}.cpp`: opaque consumer, AST/buffer lifetimes,
  moves, context isolation, diagnostic propagation and callback failure tests.
- `compiler/src/codegen/target.{hpp,cpp}`: explicit target setup, native CPU/features,
  foreign generic baseline, PIC/Small policy, module layouts and failure diagnostics;
  `target_backends.cpp`: once-only initialization of configured SDK backends.
  `tests/compiler/codegen/target.cpp`: native/moved machines, cross-target 32/64-bit
  layouts, triple normalization, unknown architectures and unavailable backends.
- `abi/include/erlang_aot/abi/{v1.hpp,term.hpp}`: namespaced C++23 term/context/
  generated-function declarations and checked target-width immediate integer codecs.
  `tests/abi/integers.cpp`: boundaries, signed round trips, overflow and wrong-tag checks.
- `abi/include/erlang_aot/abi/{features.hpp,feature_diagnostic.hpp,status.hpp}`:
  stable deferred-feature catalog/owner/boundary/step/test metadata, escaped context
  formatting and scoped fixed-width Status; `docs/features.md` records integration and propagation.
- `compiler/src/codegen/features.{hpp,cpp}`: fail-once batch reporter, owned context,
  stderr delivery and reported flag; `runtime/include/erlang_aot/runtime/features.hpp`,
  `runtime/src/diagnostics/features.cpp`: per-operation sink/report latch, scoped Status,
  stderr default and exception containment without LLVM.
  `tests/{abi,compiler/codegen,runtime}/features.cpp` and `tests/abi/feature_output.cmake`:
  catalog compatibility, all entries, context/errors and subprocess output/silence checks.
- `compiler/src/codegen/term_abi.{hpp,cpp}`: target-derived LLVM word/function types;
  `tests/compiler/codegen/term_abi.cpp`: native/cross layouts, signed LLVM constants,
  C-convention object emission and missing-target errors.
- `tests/runtime/term_layout.cpp`: compile private prefix assertions and test ABI/tag agreement.
- `compiler/src/codegen/emission.{hpp,cpp}`: fresh batch verification, cloned IR,
  legacy target emission and transactional in-memory object buffers;
  `tests/compiler/codegen/emission.cpp`: native/cross object inspection, repeat emission,
  stale verification rejection and recoverable assembler-error cleanup.
- `compiler/src/codegen/verification.{hpp,cpp}`: mandatory pre-emission verification
  gate for target settings, function bodies and whole modules; owned errors invalidate
  batch outputs. `tests/compiler/codegen/verification.cpp`: IRBuilder synthetic IR,
  malformed bodies/globals, post-verification mutation, target mismatches and failure latching.
- `compiler/CMakeLists.txt`: frontend, private codegen library and executable;
  `runtime/CMakeLists.txt`: runtime archive and `ErlangAoT::generated_program` link interface;
  `abi/CMakeLists.txt`: header-only ABI interface.
- `runtime/include/erlang_aot/runtime/{runtime,process_context}.hpp`: sole C++ lifecycle API, host owners, identities
  and lifetime tokens; `runtime/src/runtime{.cpp,_state.hpp}`: startup/shutdown, identity
  allocation and reserved code/atom ownership. The former C lifecycle adapter is removed.
  `runtime/src/process/{context,ownership,storage}.cpp`: token invalidation, transactional
  context registry and empty mailbox lifetimes. `docs/runtime-lifecycle.md`: contract.
  `tests/runtime/lifecycle_failure.cpp`: allocation rollback and registry/publication
  failure sweeps; `link.cmake`/`link_consumer.cpp`: LLVM-free consumer lifecycle,
  dispatch/copy/pinning/silence and mandatory-target/missing-runtime link validation.
- `runtime/src/memory/heap.cpp`: lazy heap lifecycle, checked allocation rejection,
  unavailable collection and word accounting; `heap_policy.hpp`: byte-budget validation;
  `copy.cpp`: immediate-only heap add/Term::copy_to. `docs/runtime-memory.md`: current
  boundaries and future roots, alignment, transit and C++ resource teardown contracts.
  `tests/runtime/memory.cpp`: budgets, overflow, invalid words/slots and deferred allocation;
  `lifecycle_failure.cpp` also checks memory operations under forced host allocation failure.
- `runtime/include/erlang_aot/runtime/process_state.hpp`: shared process enums/StepResult;
  `scheduler.hpp`: SchedulerService and lifecycle/error API. `runtime/src/scheduler/`
  `state.hpp`: private identity/metadata registry; `registry.cpp`: once-only publication,
  lookup, removal and admission; `transitions.cpp`: checked dispatch/return/suspension.
  Runtime state owns the service and clears it before context/code teardown.
  `docs/runtime-scheduler.md`: implemented boundary and reserved execution contracts;
  `tests/runtime/scheduler.cpp`: invalid/stale/foreign states, growth and synthetic teardown;
  `lifecycle_failure.cpp`: scheduler registration allocation rollback/retry.
- `runtime/include/erlang_aot/runtime/{base_types,terms}.hpp`: shared word/tag/error
  definitions and checked immediate word API. `runtime/src/terms/immediate.cpp`:
  structural classification and native ABI integer encoding/decoding without LLVM.
  `runtime/include/base_types.hpp` forwards sketch consumers to the canonical types;
  `runtime/include/terms.hpp` retains the proposed TermFactory; canonical terms.hpp
  declares Term, with immediate-only operations in src/terms/term.cpp.
  `runtime/src/terms/term_layout.hpp`: private heap prefixes and layout assertions.
  `docs/runtime-terms.md`: implemented word boundary; `runtime/design/terms.md`:
  remaining host ownership, heap/GC and immutable-value contract.
  `tests/runtime/immediate.cpp`: boundaries, malformed immediates, heap-tag rejection;
  `tests/compiler/codegen/runtime_terms.cpp`: independent LLVM constant agreement.
- `tests/runtime/{CMakeLists.txt,term_tag.cpp}`: native CTest `runtime_term_tag`, available
  with the runtime independently of the compiler; explicit expected values cover all 64 tag combinations.
- `runtime/include/binary_heap_object.hpp`: shared binary objects owning immutable
  `std::vector<Word>` storage, checked creation/errors, word views and exact bit-length/tail
  metadata. API sketch listed for IDE navigation; no binary heap or pool service.
- `runtime/include/atom_storage.hpp`, `runtime/design/atom_storage.md`: runtime-local atom interning/lookup API,
  startup caps, immutable monotonically assigned IDs and GC/compaction placeholder.
- `runtime/design/processes.md`: process/scheduler manual-review contract and decisions;
  `process_heap.hpp`: owned term storage/addition, chunked growth and collection boundary;
  `runtime/include/process.hpp`: continuation/reductions, owned signal inbox,
  deferred signal handling and process state; context declarations moved to the host API;
  `runtime/include/mailbox.hpp`: selective receive, async wait, removal and private handled-message append;
  `runtime/include/scheduler.hpp`: worker/pool lifecycle, signal servicing and process-control API.
  Worker/receive declarations remain sketches; SchedulerService and heap/mailbox lifecycle are implemented.
- `runtime/include/erlang_aot/runtime/{callable,code_server}.hpp`: exact generic keys,
  frozen module registries, code-image ownership and pinned checked calls; former
  top-level headers forward here. `runtime/src/builtins/registry.cpp`: registration;
  `invocation.cpp`: validation, exceptions and once-only unavailable reports;
  `bridge.cpp` + `abi/include/erlang_aot/abi/builtins.hpp`: status/word service bridge.
  `runtime/src/modules/code_server.cpp`: native publication and generic resolution.
  `docs/runtime-builtins.md`: implemented boundary; `runtime/design/code_server.md`:
  wider typed/atom/concurrency proposals, with typed sketches under include/unverified/.
  `tests/runtime/{builtins,builtin_bridge}.cpp`, `builtin_output.cmake`: signatures,
  freeze, pinning/capture lifetimes, failure propagation and diagnostic count/silence.
- `src/main.cpp`: help/exit contract; `src/driver/options.{hpp,cpp}`: CLI configuration/
  validation; `src/driver/frontend.{hpp,cpp}`: shared per-file loading, PP/parser,
  diagnostic callback and printing, with a positional-mode adapter and a compile
  placeholder for successfully parsed modules in the default pipeline; verbose
  stage/file tracing stays on stderr, outside project diagnostic wrappers.
- Public `{source,token,diagnostic,directive,preprocessor,features,parser,printing}.hpp`:
  source/token/events, immutable features, parser ownership/limits/results and output APIs.
- `src/source/source.cpp`: decoding/positions; `src/diagnostics/diagnostic.cpp`: logical
  locations, physical traces and opener rendering; `diagnostic.hpp` keeps Diagnostic
  aggregate data separate from DiagnosticError/LexicalError standard exceptions.
- `src/lexer/{lexer,numbers,literals}.cpp`: incremental scanning, arbitrary numeric
  values, strings/sigils and keyword state.
- `src/parsing/{probe.cpp,boost_parser.hpp}`: Boost boundary;
  `{token_cursor,token_syntax,operator_info,delimiters}.{hpp,cpp}`: shared token mechanics.
- `src/preprocessor/preprocessor.cpp`: DirectiveReader and form recovery;
  `directives.cpp`, `cursor.hpp`: directive envelopes; `engine.hpp`, `session.cpp`:
  semantic session; `conditions.cpp`, `includes.cpp`, `features.cpp`, `builtins.cpp`:
  conditionals, include frames/loaded-file observation, features and contextual definitions.
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
  attributes,types,specifications}.cpp`: iterative parenthesized AST output, named
  fields and two-space indentation; escaped strings for printable integer lists.
- `tests/cli.cmake`: CLI contracts; `tests/compiler/frontend_cases.cmake` and
  `tests/fixtures/parser/cli/`: exact source/AST/diagnostic/status regressions.
  `printing_roundtrip.cmake`: CLI source printing/reprocessing equivalence;
  `preprocessor/workflow.cmake`: includes/options/conditions/depth/truncation workflows.
- `tests/compiler/parser/{tokens,ast,forms,expressions,clauses,structural,binaries,
  control,exceptions,comprehensions,attributes,types,specifications}.cpp`: retained
  API-only limits, ownership, provenance, rollback and invalid-handle invariants.
  `hardening.cpp`: injected ceilings/EOF; `stress.cmake`: bounded source CLI stress;
  `mutations.cmake` + `tests/fixtures/parser/mutations.json`: deterministic 900-case
  source mutation/recovery corpus; `consumer.{cpp,cmake}`: separately built public
  frontend consumer retaining syntax after sessions and source managers die.
- `tests/compiler/parser/{dump.cpp,operators.hpp,terms_dump.hpp,types_dump.hpp}`,
  `record_printer.hpp`, `oracle.escript`: native/OTP parenthesized AST projections;
  RAW/EPP keep Erlang-term syntax. `tests/compiler/encoding.hpp`:
  shared exact encodings; `{reference,oracle,phase1,phase2}.cmake`: existing suites.
- `tests/compiler/parser/historical.cmake`: seed AST closure and offline inventory audit;
  `coverage.{cmake,escript}`: measured pinned reductions; `pinned.cmake`: source verification;
  `corpus.cmake`: separate real-source preprocessing/parsing/determinism checks.
- `tests/fixtures/parser/`: immutable original records and phase-specific probes;
  `phase5/coverage.tsv`: attribute/type row index; `phase6/`: measured full inventory,
  closure fixtures, historical ASTs and checksum-pinned real OTP source manifest.
- `docs/{preprocessor,parser,parser-validation}.md`: contracts and evidence;
  `references/otp`: ignored research checkout.
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
- `tests/compiler/project/{cli,workflow}.cmake` include `{manifest,selection,
  discovery,options}_cases.cmake`: real multi-target/schema/options/creation/race,
  discovery/alias/native-path/corpus and no-write workflows. `limits.cpp` retains
  injected budgets/read failure/invalid UTF-8/native syntax; `creation_failure.cpp`
  retains injected partial-write/close cleanup. `support.hpp`: active assertions.
- `docs/projects.md`: delivered format, precedence, discovery and creation workflows;
  `docs/project-validation.md`: C++23 evidence and pending host matrix;
  `examples/project/{project.toml,src/main.erl}`: runnable two-target frontend example.
- `.agents/00-finished.md`: compact foundations, preprocessor/OTP inventory, parser,
  project and test migration archive, historical evidence and remaining obligations.
  `docs/test-migration.md`: case-level disposition and validation evidence.
  `.agents/04-compile.md`: remaining compiler steps route tests to observable workflows.
- `tests/runtime/link_consumer.cpp`, `link.cmake`: independently configured runtime
  startup/context/dispatch/copy/publication/pinning/teardown integration; term layout
  is a build-only object. `tests/fixtures/runtime/diagnostics/`: exact stderr context.
  `cmake/TestHost.cmake.in`: propagate host compiler/CRT/sanitizer flags to consumers.
  `compiler/erlangaot.manifest`: Windows UTF-8 argv; `.gitattributes`: byte-exact fixtures.

- `runtime/src/terms/factory.cpp`: lifetime-checked TermFactory reporting placeholders.
  `runtime/src/{process,scheduler,modules}/services.cpp`: deferred send, run/execute
  and unload entry points. `runtime/include/erlang_aot/runtime/features.hpp` maps
  shared diagnostic status into typed host failures; memory/heap.cpp now reports.
  `runtime/include/erlang_aot/runtime/builtins.hpp`: bounded exact known-BIF catalog;
  builtins/bridge.cpp distinguishes unknown_builtin from known deferred signatures.
  `tests/runtime/services.cpp`, `service_output.cmake`: real boundary/sink/state/
  cleanup and once-only stderr tests. `docs/runtime-services.md`: scope and contracts.

- `compiler/src/driver/command.{hpp,cpp}` owns CLI help and positional/project dispatch;
  `main.cpp` contains unexpected failures. `preprocessor/integer.cpp` owns decimal
  integer formatting and exact finite binary64-to-integer conversion.

- `compiler/src/semantic/declarations.{hpp,cpp}` indexes modules/functions/exports
  and renders located diagnostics; `symbols.{hpp,cpp}` provides reversible identities.
  `{capabilities,expression_capability,literals}.{hpp,cpp}` enforces the subset;
  `features.{hpp,cpp}` maps failures to the shared catalog. `bindings.{hpp,cpp}`
  resolves parameter positions; `calls.{hpp,cpp}` resolves direct calls and ordering.
  `driver/frontend.cpp` owns batches; `project/execution.{hpp,cpp}` dispatches each target.
- `compiler/src/semantic/types/`: `domain.{hpp,cpp}` owns type identities, joins and
  limits; `syntax.{hpp,cpp}` exhaustively describes/translates AST type categories.
  `declarations.{hpp,cpp}` owns the registry and resolves bodies/contracts; `collect.cpp`
  indexes declarations/exports/callback metadata. `resolver.{hpp,cpp}` resolves nodes
  and visibility; `traversal.cpp` owns bounded traversal, scopes and union-use counting.
  `constants.cpp` evaluates exact bounds; `expansion.cpp` memoizes bounded substitution.
- `tests/compiler/semantic/cases.cmake` extends real frontend/project CLI workflows.
  `symbols.cpp`, `types.cpp`, `declared_types.cpp` retain identity/lattice/ownership/
  opacity invariants until emitted artifacts or step 39 type inspection replace them.
  `docs/semantic.md` documents the implemented analysis boundary.
- `codegen/lowering_boundaries.{hpp,cpp}` invalidates staged results for deferred operations.

- `compiler/src/codegen/lowering.{hpp,cpp}`: validated-batch declarations, body dispatch
  and verification. `lowering_expressions.{hpp,cpp}` lowers literals/parameters and
  walks nested calls; `lowering_state.hpp` retains iterative expression state.
  `lowering_calls.cpp` emits generic calls and imports resolved remote exports.
  `tests/compiler/codegen/lowering.cpp` and `tests/fixtures/codegen/` provide the
  real-source backend adapter, target-width/ABI checks and object symbol inspection.

- `abi/include/erlang_aot/abi/modules.hpp`: descriptor layout and generated registration ABI.
  `codegen/module_registration.{hpp,cpp}` emits descriptors, startup and service references.
  `runtime/include/erlang_aot/runtime/modules.hpp`, `runtime/src/modules/registration.cpp`: descriptor validation,
  generic marshaling and transactional publication. `docs/runtime-modules.md`: contract.
  `tests/compiler/codegen/registration*` links real generated objects with/without runtime;
  `tests/runtime/registration.cpp` checks rejection and lifetime ownership.

- `codegen/specialization.{hpp,cpp}`: canonical profiles, decisions and bounded selection.
  `specialization_analysis.{hpp,cpp}`: real inference profiles and generic IR measurements.
  `integer_guards.{hpp,cpp}`: exact side-effect-free argument tag-check recognition
  using LLVM use lists and instruction comparison. `docs/specialization.md`: policy.
  `tests/compiler/codegen/specialization.cpp`: stress limits and real-source no-benefit cases.

- `codegen/specialization_lowering.{hpp,cpp}`: measured draft admission and public-entry
  replacement. `specialization_cloning.cpp`: LLVM cloning and proven-check removal;
  `specialization_dispatch.cpp`: bounded low-tag guards and generic fallback.
  `tests/compiler/codegen/specialization_{emit,consumer}.cpp`: focused synthetic IR
  and separately linked native guard/fallback equivalence; `guards.erl` supplies
  real semantic/descriptor setup without adding supported source guard syntax.

- `codegen/optimization.{hpp,cpp}`: verified standard LLVM PassBuilder O0/O2.
  `tests/compiler/codegen/registration*`: native runtime consumer at both levels.

- `codegen/serialization.{hpp,cpp}`: verified snapshots and assembly/bitcode writers.
  `tests/compiler/codegen/serialization.cpp`: real-source SDK round trips and invalid-IR rejection.

- `compiler/src/artifacts/{artifacts,paths,replace}.*`: staged writes, portable
  semantic names/alias preflight, and platform complete-file replacement.
  `tests/compiler/codegen/artifacts.cpp`: filesystem preservation/failure cases.

- `driver/backend_options.{hpp,cpp}`: compilation operands and action conflicts.
  `tests/compiler/codegen/options.cmake`: public positional/project option matrix.
