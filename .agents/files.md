# File map

- `docs/compile.md`: pinned SDK provenance/tools, frozen compilation subset, provisional
  ABI and command/artifact contract; `.agents/04-compile.md`: ordered implementation plan.

Paths are repository-relative; `src/` in compiler entries means `compiler/src/`.
Public headers live in `compiler/include/erlang_aot/compiler/`.

- `CMakeLists.txt`, `CMakePresets.json`, `Makefile`, `run-macos.sh`: component and
  configuration (root CMake requires C++23 without extensions in all subdirectories
  and supplies shared Boost system includes for IDE header analysis),
  parallel builds, test/format targets, transparent macOS runner.
- `cmake/ProjectOptions.cmake`: target warnings as errors; `BoostDependencies.cmake`:
  shared installed/Homebrew/local Boost discovery and Multiprecision interface target,
  including SYSTEM classification of Homebrew's matching linked include alias;
  `CompilerDependencies.cmake`: compiler-only Parser discovery; `ErlangDependencies.cmake` and
  `ErlangVersion.escript`: host OTP discovery/version checks.
- `cmake/{CheckComplexity,CheckClangTidy}.cmake`, `QualityToolchain.cmake.in`,
  `.clang-{format,tidy}`, `tools/requirements-quality.txt`: required quality policy.
- `cmake/LLVMDependencies.cmake`, `LLVMPolicy.cmake`, `probes/llvm.cpp`: global-only
  LLVM 23.1.x discovery, path/version policy, host ABI link probe and available
  X86/ARM/AArch64 backend selection/component linkage.
- `compiler/src/codegen/sdk.{hpp,cpp}`: private SDK version boundary;
  `tests/compiler/codegen/{sdk.cpp,dependency.cmake,CMakeLists.txt}`: linked smoke,
  discovery/rejection fixtures and LLVM-independent runtime configuration/build.
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
  `tests/runtime/{lifecycle,lifecycle_failure}.cpp`: lifetimes/errors and allocation rollback and registry/publication failure sweeps;
  `lifecycle_output.cmake`: silence; `link.cmake`/`link_consumer.cpp`: LLVM-free consumer
  link/run through the mandatory target and missing-runtime link failure.
- `runtime/src/memory/heap.cpp`: lazy heap lifecycle, checked allocation rejection,
  unavailable collection and word accounting; `heap_policy.hpp`: byte-budget validation;
  `copy.cpp`: immediate-only heap add/Term::copy_to. `docs/runtime-memory.md`: current
  boundaries and future roots, alignment, transit and C++ resource teardown contracts.
  `tests/runtime/memory.cpp`: separate owners/budgets, overflow, copy and exit behavior;
  `lifecycle_failure.cpp` also checks memory operations under forced host allocation failure.
- `runtime/include/erlang_aot/runtime/process_state.hpp`: shared process enums/StepResult;
  `scheduler.hpp`: SchedulerService and lifecycle/error API. `runtime/src/scheduler/`
  `state.hpp`: private identity/metadata registry; `registry.cpp`: once-only publication,
  lookup, removal and admission; `transitions.cpp`: checked dispatch/return/suspension.
  Runtime state owns the service and clears it before context/code teardown.
  `docs/runtime-scheduler.md`: implemented boundary and reserved execution contracts;
  `tests/runtime/scheduler.cpp`: lifecycle, isolation, growth and ordered teardown;
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
  locations, physical traces and opener rendering.
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
