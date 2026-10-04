# File lookup

Repo-relative paths. File keys omit `.cpp`/`.hpp`; `{a,b}` groups siblings, `*` groups a family.
**C** = `compiler/src/`, **R** = `runtime/src/`; **+** = planned, create only with implementation.
[Architecture](arch.md) · [Backlog](01-todo.md) · [Remaining plan](11-plan.md) · [History](00-finished.md).

## Placement

| Kind | Home |
| --- | --- |
| Compiler API / AST | `compiler/include/erlang_aot/compiler/`, `ast/` beneath it; `mangling`: compile-time Itanium/MSVC symbols for runtime services |
| Runtime API | `runtime/include/erlang_aot/runtime/` |
| Shared generated-code ABI | `abi/include/erlang_aot/abi/`: `v1`, `term`, `status`, `calls`, `modules`, `builtins`, `equality`, `containers`, `integers`, `floats`, `maps`, `bits`, `immediate_services`, `output`, `startup`, `features`, `feature_diagnostic` |
| Private headers | Beside owning source; project-internal C++23; runtime stays LLVM-free |
| Sketches / proposals | `runtime/include/*.hpp` (also legacy forwarders), `runtime/include/unverified/`; production APIs go in the canonical tree |
| New sources / tests | Register in owning `CMakeLists.txt`; behavior tests through CLI/native workflows, private tests for inaccessible invariants |

## Compiler — C

Keys in the last column are relative to the directory column.

| Directory | Owns | File keys |
| --- | --- | --- |
| `source/`, `diagnostics/` | Buffers, positions / diagnostic provenance | `source`, `diagnostic`, respectively |
| `lexer/` | Tokens, numbers, strings/sigils | `lexer`, `numbers`, `literals` |
| `parsing/` | Boost boundary, token mechanics | `probe`, `boost_parser`, `token_{cursor,syntax}`, `operator_info`, `delimiters` |
| `preprocessor/` | Directives, sessions, includes, macros | `preprocessor`, `directives`, `engine`, `cursor`, `session`, `conditions`, `includes`, `features`, `builtins`, `macros`, `arguments`, `token_utils` |
| `preprocessor/` | Closed preprocessing evaluator | `expression*`, `operators`, `guards`, `terms`, `value`, `bits`, `integer` |
| `ast/` | Owned arenas, IDs, transactions, shape checks | `arena`, `storage`, `builder`, `module`, `children`; category files mirror syntax |
| `parser/` | Erlang grammar, recovery, budgets | `parser`, `forms`, `diagnostics`; `expressions`, `clauses`, `literals`, `aggregates`, `maps`, `records`, `structural`, `binaries`, `control`, `funs`, `exceptions`, `comprehensions` |
| `parser/` | Attributes, records, literal terms, types/specs | `attributes`, `declarations`, `documentation`, `attribute_*`, `term_*`, `types`, `type_*`, `specifications` |
| `printing/` | Source/token/AST output | `source`, `token_text`, `printable`, `tree*` |
| `driver/` | CLI, frontend, analysis/backend, entry resolution, escript headers, publication | `command`, `options`, `frontend`, `analysis`, `entry`, `escript`, `backend*`, `project_backend`, `publication`; `C/main.cpp`: entry/failure boundary |
| `driver/` | Progress, IR/type inspection, debug options | `progress`, `display`, `inspection`, `type_*`, `implementation_debug`; shared selector: `C/implementation_debug.hpp` |
| `project/` | TOML/schema; discovery/options; target execution | `model`, `loader`, `diagnostics`, `decode*`, `schema`; `paths`, `glob*`, `discovery`, `sources`, `identity`, `selection`, `options`; `entry` (MODULE[:FUNCTION] spelling), `plan`, `execution`, `cli`, `command`, `template`, `create`; `cmake/Dependencies.cmake`: toml++ |
| `semantic/` | Symbols, calls, executable admission, escript rules | `declarations`, `escript`, `symbols`, `calls`, `capabilities`, `expression_capability`, `literals`, `features` |
| `semantic/` | Scoped bindings, normalized patterns, match plans | `bindings`, `binding_*`, `patterns`, `pattern_*`, `match_plan`, `match_plan_internal`, `match_plan_containers`, `match_plan_bits`, `binary_options`, `records`, `match_plan_records` |
| `semantic/` | Guard legality/resolution, service availability | `services`, `guard_analysis`, `immediate_services`, `service_metadata` |
| `semantic/types/` | Type declarations, bounded inference/contracts | `domain`, `syntax`, `declarations`, `collect`, `resolver`, `traversal`, `constants`, `expansion`, `inference`, `inference_bindings`, `contracts`, `membership`, `trace` |
| `codegen/` | LLVM ownership, target/ABI, diagnostics, runtime-service symbols | `request`, `output`, `result`, `compilation`, `llvm_state`, `sdk`, `diagnostics`, `target*`, `term_abi`, `runtime_symbols` |
| `codegen/` | Bodies/calls, matching, guards, eager/lazy flow | `lowering`, `lowering_{boundaries,clauses,expressions,state,calls,roots,match,body_match,immediates,containers,integers,floats,maps,bits,records,record_tests,guards,walk}` |
| `codegen/` | Atom slots / registration; startup module (`main` → `erlang_aot_main_v1`); guarded variants | `module_{atoms,registration}`, `startup`; `specialization*`, `integer_guards` |
| `codegen/` | Verify/optimize/emit; limits/reporting; provenance | `verification`, `optimization`, `emission`, `serialization`; `limits`, `bounded_stream`, `features`, `progress`; `source_{locations,annotations}` |
| `artifacts/` | Staged writes, safe names, file replacement | `artifacts`, `paths`, `replace` |
| `linking/` | Executable linking: staging/publication, Clang discovery/run, runtime archive lookup and target check | `link`, `toolchain`, `runtime_library` |

## Runtime — R

Keys are relative to the directory column. Stable backing and roots are implemented; compound admission, GC, workers and messages have separate owners.

| Directory | Owns | File keys |
| --- | --- | --- |
| `.` | Runtime lifecycle/shared state | `runtime`, `runtime_state` |
| `process/` | Context/heap/mailbox ownership, checked error transport | `context`, `ownership`, `storage`, `generated_calls`, `roots`, `services` |
| `memory/` | Stable backing, budgets, rollback/resource teardown; copying boundary | `heap`, `heap_policy`, `heap_storage`, `heap_reservation`, `heap_object`, `heap_terms`, `heap_publication`, `copy` |
| `terms/` | Words/Terms, constructors/layouts, atoms | `immediate`, `term`, `factory`, `container_factory`, `container_access`, `term_layout`, `atoms`, `atom_spelling` |
| `terms/` | Equality, ordering, immediate services | `equality`, `immediate_order`, `structural_order`, `immediate_services`, `container_services`, `service_errors` |
| `terms/` | `~w`/display text: traversal, scalar rules | `term_text` (frames, `TextOutput`), `term_text_scalars` (atoms, floats, bits, display strings); API `output.hpp` |
| `terms/` | Canonical arbitrary integers, exact operations and checked transport | `integers`, `integer_{access,values,decimal,words,sum,factory,operations,service,literal}` |
| `terms/` | Finite binary64 construction/conversions, mixed arithmetic/order | `floats`, `float_{factory,literal,operations}`, `numeric_{conversions,order,service}` |
| `terms/` | Immutable exact-key maps, staged updates, checked service transport | `maps`, `map_{access,factory,services}`; compiler `semantic/{pattern_reads,match_plan_maps}`, `codegen/lowering_maps` |
| `terms/` | Immutable packed bitstrings, shared views, numeric/UTF segments and checked cursors | `bitstrings`, `bit_{access,factory,numeric,float,utf,services}`; compiler `semantic/{binary_options,match_plan_bits}`, `codegen/lowering_bits` |
| `scheduler/` | Process records/transitions, execution boundary | `state`, `registry`, `transitions`, `services` |
| `builtins/` | Generic registry, checked invocation/ABI bridge; standard output and `erlang_aot_display_v1` | `registry`, `invocation`, `bridge`, `output`; known-BIF catalog: canonical API `builtins.hpp` |
| `modules/` | Code pins, publication, descriptors, atom bindings | `code_server`, `registration`, `atoms`, `services` |
| `startup/` | Program startup `erlang_aot_main_v1` (ABI checks, registration, entry, exit status), argv decoding, `erlang_aot_halt_v1` | `startup` (+ private `startup.hpp`), `arguments`, `halt` |
| `diagnostics/` | Runtime feature reporting | `features` |

## Build, support, evidence

| Location | Lookup |
| --- | --- |
| Root | `CMakeLists.txt`, `CMakePresets.json`, `Makefile`, `make-*.bat`, `run-macos.sh`: build/test/format; `erlangaot.bat`: run wrapper; `.clang-{format,tidy}`: style/quality |
| Component CMake files | `compiler/`: frontend, semantic/backend, `erlang_aot` → `erlangaot`; `runtime/`: `erlang_runtime`, `ErlangAoT::generated_program`; `abi/`: headers; `tests/`: opt-in CTest |
| `cmake/` | `ProjectOptions.cmake`; `{Boost,Compiler,Erlang,LLVM,Zlib,Zstd}Dependencies.cmake`; `LLVM{Policy,Downloads}.cmake`; `Windows{Toolchain,Host,DependencyBuild}.cmake`; `ThirdPartyDependencies.cmake`; `ErlangVersion.escript` |
| `cmake/` checks | `Check{Complexity,ClangTidy}.cmake` (changed or all scope via `QualityScope.cmake` + `quality_scope.py`), `{QualityToolchain,TestHost}.cmake.in`; `modules/Find{ZLIB,zstd}.cmake`; `probes/{windows,llvm}.cpp`; `tools/requirements-quality.txt` |
| `.agents/`, root guidance | Plans/map/history; `AGENTS.md`: instructions; `README.md`: usage; `.agents/aimemory.md`: AI notes |
| `runtime/design/` | `{terms,processes,atom_storage,code_server}.md`: design contracts/proposals |
| `docs/` | Brief reference notes indexed by `docs/README.md`: frontend (`preprocessor`, `parser`, `projects`), compiler (`compile`, `executables`, `semantic`, `specialization`, `abi`, `features`), language (`patterns`, `guards`, `terms`), `runtime.md`, `otp-reference.md`, `validation.md` (baseline, test design, history) |
| `references/` | `otp-pin.cmake`: maint-29 revision; ignored `otp/`: checkout and generated OTP headers; procedure: `docs/otp-reference.md`; gate: `tests/compiler/parser/pinned.cmake`. Preserve historical evidence revisions. |
| `examples/` | `compile/`: remote scalar/container/record classification and native harness; `project/src/`: manifest example; future runnable demos: `<feature>/` |
| Local/generated | `build/`: outputs/logs; `thirdparty/`: SDK/dependencies and `tools/erlfmt/` formatter; `.venv-quality/`: quality tools; editor state stays local |

## Tests / fixtures

Compiler runners: `tests/compiler/<area>/`; source/expected data: `tests/fixtures/<area>/`.
Existing fixture areas: `{preprocessor,parser,project,codegen,patternmatch,runtime,programs,printing}`.

| Area | Lookup / placement |
| --- | --- |
| `preprocessor`, `parser` | CLI/OTP/grammar/corpus; parser `pinned.cmake`, `corpus.cmake`, `coverage.*`, `historical.cmake`; shared `tests/compiler/{frontend_cases,printing_roundtrip}.cmake`, `tests/cli.cmake` |
| `project` | `cli.cmake`, `workflow.cmake`, `*_cases.cmake`; injected `limits.cpp`, `creation_failure.cpp` |
| `semantic` | `cases.cmake`: CLI diagnostics; binding/pattern/type/symbol invariants; source fixtures stay in the relevant existing area |
| `patternmatch` | `evidence.py`, `oracle.escript`, `atoms.*`, `bindings.*`, `patterns.*`, `immediate.*`, `services.py`, `booleans.py`, `clauses.py`, `sequences.py`, `containers.py`, `integers.py`, `floats.py`, `maps.py`, `bits.py`, `records.py`, `guard_catalog.py`, `facts.py`, `closure.py`: conservative proofs and seeded/provenance closure; native bounded value transport: `codegen/match_wire.hpp` |
| `codegen` | `native*`, `differential.py`, `execution_oracle.escript`, `cross_targets.py`; inspection/resource/publication checks; `atoms*`, `match*`, `failure_*`, `service_*`: runtime integration |
| `programs` | End-goal projects (`textstats`, `frames`, `avltree`, `ring`, `kvstore`, `supervise`) with feature map README; `fixtures.py` hashes, `programs.py` CTest (golden hashes + exact `compile.txt`), `regenerate.py` + `oracle.escript` explicit OTP goldens |
| `printing` | `values.py` (authored values, corpus collection, wire parse, order rule), `regenerate.py` + `oracle.escript` (explicit OTP goldens), `display.py` CTest `printing_display` (compiled display calls via `codegen/match.cmake`); runtime goldens `tests/runtime/printing.cpp` |
| `tests/runtime/`, `tests/abi/` | Runtime-only lifecycle/ownership/services (`link.cmake`, `link_consumer.cpp`); ABI codecs/layout/catalog. Keep runtime-only tests LLVM-free. |
| `linking` | F01 entry selection, escripts, startup objects and `-o` linking (`entry.cmake`, `escript.cmake`, `startup.cmake`: manual CMake link, exit paths, startup IR; `executable.cmake`: `-o` example/argv/escript runs and toolchain/destination failures; fixtures `tests/fixtures/linking/{entry,escript,startup}/`); runtime-only startup rejections `tests/runtime/startup.cpp`; later link workflows (F32/D01) join here |
| **+** `transforms`, `stage_writers`, `stage_readers` | D03–D05 selected workflows; runners/fixtures follow area convention; reserved until selected |
| **+** `tests/interop/` | D06 independent external consumers |

## Backlog → owners

Common wiring: validation → `C/semantic`; facts → `C/semantic/types`; LLVM → `C/codegen`;
options → `C/driver` + `C/project`; output publication → `C/artifacts`; API/ABI → homes above.
Tests use the owning area above; generated-program behavior → `codegen`, runtime behavior → `tests/runtime/`.
All IDs from `01-todo.md`; partial features extend existing owners; D-items remain optional.

| Feature(s) | Main / additional destinations |
| --- | --- |
| F01 executable startup; F32 LTO | `C/linking`; `C/codegen/startup`; F01 bootstrap: `R/startup`, using `R/runtime.cpp` lifecycle |
| F02 roots/safepoints | `C/codegen`, `R/memory`, `R/process`, shared ABI |
| F03 heaps; F04 GC; F05 graph copying | `R/memory`: allocation/tracing/copying; `R/terms`: constructors/layout traversal/destruction |
| F06 atoms | `R/terms`: synchronization; `R/modules`: bindings; `C/codegen/module_atoms` |
| F07 process/port/reference IDs | `R/terms`: representation; `R/process`: owners/lifetimes; `R/scheduler`: lookup/routing |
| F08 containers; F09 bitstrings; F10 integers; F11 floats; F12 equality/order | `R/terms`: values/operations; `R/memory`: tracing/copying; common semantic/lowering wiring |
| F13 patterns; F14 guards; F17 records | `C/semantic`, `C/codegen`, `R/terms`; F14 services also `R/builtins` |
| F15 clauses; F16 control flow; F21 recursion/tail calls | `C/semantic`, `C/semantic/types`, `C/codegen`; F21 continuations: `R/process` |
| F18 closures; F19 dynamic calls | `C/semantic`, `C/codegen`, `R/terms`, `R/modules`; F18 capture tracing/copying: `R/memory` |
| F20 exceptions | `C/semantic`, `C/codegen`, `R/process/generated_calls`, shared ABI |
| F22 cooperative execution | `R/process`, `R/scheduler`, `C/codegen`, shared ABI |
| F23 workers/wakeups | `R/scheduler`; synchronize service owners in `R/{process,terms,modules}` |
| F24 signals/send | `R/process`: inbox/mailbox; `R/memory`: transfer; `C/codegen`: send |
| F25 receive/timeouts | `R/process`: cursors/arrival; `R/scheduler`: timers/wakeup; `C/{semantic,codegen}`: selection/resumption |
| F26 builtins; F27 typed/native callables | `R/builtins`: wrappers/conversions; algorithms stay with value/process owners; F27 retained code: `R/modules` |
| F28 concurrent code server | `R/modules`; atom coordination: `R/terms`; shutdown: `R/scheduler` |
| F29 specialization | `C/codegen/specialization*`, `C/codegen/integer_guards`, `C/semantic/types` |
| F30 debug info | `C/codegen/source_locations` + metadata; `C/linking` |
| F31 profiling | `C/codegen`: instrumentation; **+** `R/profiling`: collection/attribution/export |
| V01 native matrix; V02 sanitizers | `cmake/`, `CMakePresets.json`, root runners, owning tests, `docs/` evidence |
| V03 OTP evidence | `references/`, parser/patternmatch/codegen runners + fixtures, `docs/otp-reference.md` |
| V04 test migration | Owning tests/CMake registrations; `docs/validation.md#test-design` |
| D01 dynamic modules/upgrades | `R/modules`, `C/linking`, shared ABI |
| D02 atom collection | `R/terms`; roots/resources: `R/{memory,modules,process}` |
| D03 attributes/transforms | `C/semantic`; **+** `C/transforms`; invocation: `C/{driver,project}`; on-load: `R/modules` |
| D04 stage interchange | **+** `C/stage_writers/{preprocessed,abstract,ir}`; format contracts: `docs/` |
| D05 stage readers | **+** `C/stage_readers/{preprocessed,abstract,ir}` — directory reservations only |
| D06 C/FFI | **+** `R/interop`, **+** `runtime/include/erlang_aot/interop/`; concrete external use only |
| D07 project extensions | `C/project`: schema/profiles/graphs/packages/watch/cache/scheduling; `C/driver`, `C/artifacts`; project tests/fixtures/examples; `docs/projects.md` |

Local source: `tests/fixtures/patternmatch/fragments/` owns Erlang modules/includes and `corpus.json` input inventories. Golden observations: `tests/fixtures/patternmatch/generated/` owns calls, expected results and manifests. `tests/compiler/patternmatch/{authored,stored,regenerate,regenerate_cases,upstream}.py` stage, load, explicitly refresh and optionally audit them; `matrix.py` selects fast/full policy/driver combinations (`ERLANG_AOT_TEST_MODE`; label `full_only` marks full-mode-only tests); `fixture_sources.py` checks source isolation without OTP. `ERLANG_AOT_OTP_AUDITS` gates live audits; normal tests are OTP-free. Nineteen corpus manifests retain 67,634 native values and 106 semantic rows. Source audit, provenance and step history: `docs/validation.md`.
