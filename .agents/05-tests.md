# Test migration plan

Status: available migrations implemented, 2026-09-28; final validation has outstanding
Windows/parser/toolchain blockers. Backend source-to-executable migrations remain
deferred until driver integration/lowering. See [case-level coverage and evidence](../docs/test-migration.md).

The tables below preserve the **baseline inventory and intended dispositions**;
they are not current registrations. Project normal behavior now uses the CLI,
frontend source cases use exact CLI/OTP fixtures and bounded source mutations,
and runtime normal behavior uses a separately built dispatch/lifecycle consumer.
Removed suites were deleted with their registrations. Retained API-only limits,
invalid handles, raw-stage contracts, fault injection and cross-width ABI checks
are justified individually in the coverage ledger. AST/raw-attribute lifetime
checks remain pending compatible frontend sanitizer evidence.

Current registration count is 75 (baseline 93); this is inventory, not a quality
target. Runtime ASan passes all 15 tests. Full Debug/Release and quality results,
including failures, are recorded in the ledger. No clean-commit gate is claimed.
Remaining compiler steps and test retirement conditions are synchronized with
[04-compile.md](04-compile.md#test-routing-for-the-remaining-steps-1546).

## Objective and scope

Apply the testing strategy in [AGENTS.md](../AGENTS.md): minimize unit tests and maximize meaningful black-box and end-to-end coverage. Inventory below uses CTest names, not executable target names. Each row covers the existing test executable and its source; mixed component/integration tests are included so they are not mistaken for disposable unit tests.

Sources of registration: `tests/CMakeLists.txt`, `tests/compiler/{project,codegen}/CMakeLists.txt`, `tests/runtime/CMakeLists.txt`, and `tests/abi/CMakeLists.txt`. Paths in tables are relative to the stated directory. Proposed deletion always requires a case-level coverage review; similar test names alone do not prove redundancy.

The compiler CLI currently preprocesses/parses but `compiler/src/driver/frontend.cpp::compile_module` is a stub. Real Erlang-to-executable tests are therefore a future migration destination, not existing coverage. Use CLI frontend workflows now, real linked runtime consumers where appropriate, and defer backend migrations until lowering/output is reachable through the CLI. A C++ test moved behind a subprocess wrapper is still a unit test if it only checks the same internals.

Actions:

- **Migrate**: replace useful assertions with observable CLI, fixture, differential, or real-consumer scenarios, then remove the superseded unit executable.
- **Delete after coverage review**: remove redundant smoke/model checks once their behavior is demonstrated by the named workflow; preserve any unique regression first.
- **Split**: migrate normal behavior; retain only explicitly justified fault-injection, invariant, or otherwise unreachable cases.
- **Defer**: keep current useful coverage until the required product feature exists.

## Frontend inventory

Sources relative to `tests/compiler/`.

| CTest name | Source | Proposed action and replacement |
| --- | --- | --- |
| `parser_probe` | `probe.cpp` | Delete after coverage review: exercise valid/malformed macro directives and trailing input through preprocessing fixtures. Check whether the prototype probe API still has any production consumer before considering its separate removal. |
| `lexer` | `lexer.cpp` | Migrate valid tokens to scanner golden/OTP differential fixtures; malformed literals and source locations to CLI diagnostic cases. |
| `printing` | `printing.cpp` | Migrate to CLI preprocessed-output fixtures and reprocessing round trips, including sigils and macro options. |
| `printing_ast` | `printing_ast.cpp` | Migrate formatting/value/deep-tree cases to CLI AST output fixtures with deterministic expected output and timeouts. |
| `preprocessor_forms` | `preprocessor/forms.cpp` | Migrate directives, malformed envelopes, diagnostics, isolation, and bounded recovery to real files through CLI/preprocessor fixtures. |
| `preprocessor_semantics` | `preprocessor/semantics.cpp` | Migrate macros, conditions, includes, encoding/location and resource cases to golden/OTP fixtures and multi-file CLI runs; split cases requiring injected limits unavailable through the CLI. |
| `parser_tokens` | `parser/tokens.cpp` | Split: migrate source locations, include/macro provenance and diagnostic propagation to CLI fixtures; retain only necessary internal transport/cursor invariants without an observable replacement. |
| `parser_ast` | `parser/ast.cpp` | Split: use large/malformed source fixtures, recovery and sanitizer runs for growth/lifetime/provenance; delete duplicated builder-shape checks, retain justified rollback/invalid-handle invariants. |
| `parser_forms` | `parser/forms.cpp` | Migrate accepted/rejected forms and recovery to CLI AST/diagnostic and OTP fixtures. |
| `parser_expressions` | `parser/expressions.cpp` | Migrate expression and precedence cases to AST golden/OTP fixtures; later add execution comparisons when lowering exists. |
| `parser_clauses` | `parser/clauses.cpp` | Migrate clauses/guards and rejection cases to AST/OTP and CLI diagnostics. |
| `parser_control` | `parser/control.cpp` | Migrate control-expression shapes and recovery to source fixtures; eventual execution tests cover semantics separately. |
| `parser_exceptions` | `parser/exceptions.cpp` | Migrate exception syntax/recovery to AST/OTP fixtures; defer exception execution coverage until implemented. |
| `parser_comprehensions` | `parser/comprehensions.cpp` | Migrate comprehension syntax, qualifiers, and malformed input to AST/OTP fixtures with timeouts. |
| `parser_attributes` | `parser/attributes.cpp` | Split: migrate attribute payload/recovery to CLI/OTP fixtures; use repeated multi-file parsing under sanitizers for ownership checks. |
| `parser_types` | `parser/types.cpp` | Migrate type syntax and rejection to AST/OTP fixtures; delete duplicate private builder checks after review. |
| `parser_specifications` | `parser/specifications.cpp` | Migrate spec structure/recovery to AST/OTP fixtures; review builder-only checks for deletion or a small invariant exception. |
| `parser_structural` | `parser/structural.cpp` | Split: migrate records/maps and recovery to AST/OTP fixtures; retain only limits/invariants that real input cannot exercise practically. |
| `parser_binaries` | `parser/binaries.cpp` | Split: migrate segments/sigils/contexts and malformed input to AST/OTP fixtures; review synthetic invariant checks separately. |
| `parser_hardening` | `parser/hardening.cpp` | Migrate source-level stress/regression cases to a bounded CLI corpus; split any API-only limit/ownership checks. |
| `parser_mutations` | `parser/mutations.cpp` | Split: migrate representable token mutations to deterministic source/CLI corpus testing, checking recovery, bounded termination, repeatable diagnostics and crashes under sanitizers. Preserve seeds; retain injected parser-limit/token cases without an equivalent source representation. |
| `parser_consumer` | `parser/consumer.cpp` | Review as an API consumer integration test. Keep useful public-header/link/ownership contracts in a separately built consumer; delete overlapping parser-shape assertions. |

Use existing `tests/fixtures/preprocessor/` and `tests/fixtures/parser/` rather than duplicating corpora. AST/token dump executables are stage integration adapters, not full compiler end-to-end tests; prefer the product CLI when it exposes the required output.

## Project inventory

Sources relative to `tests/compiler/project/`. Main destinations are existing `cli.cmake`, `workflow.cmake`, and `tests/fixtures/project/workflow/`.

| CTest name | Source | Proposed action and replacement |
| --- | --- | --- |
| `project_dependency` | `dependency.cpp` | Delete after coverage review: valid/invalid TOML through `--project` already exercises dependency integration; do not maintain independent tests of toml++ behavior. |
| `project_model` | `model.cpp` | Delete after coverage review: defaults and independent target options belong in multi-target CLI fixtures; keep essential compile-time ownership constraints with the owning type if justified. |
| `project_loader` | `loader.cpp` | Migrate real manifest reads, missing files, and parse diagnostics to CLI fixtures. |
| `project_decode` | `decode.cpp` | Migrate schema/type/unknown-key rejection to manifest fixtures, checking exit code and diagnostic coordinates. |
| `project_decode_options` | `decode_options.cpp` | Migrate option decoding to manifest/CLI fixtures that demonstrate effects on real source processing. |
| `project_paths` | `paths.cpp` | Migrate relative/search/missing paths to temporary project directory workflows. |
| `project_glob` | `glob.cpp` | Split: migrate wildcard/Unicode/hidden-file selection to real directory fixtures; retain narrowly scoped bounded-work/invalid-encoding tests where host filesystems or CLI limits prevent reproduction. |
| `project_discovery` | `discovery.cpp` | Migrate discovery/order/deduplication to project CLI runs over real directory trees. |
| `project_sources` | `sources.cpp` | Migrate source expansion/selection/errors to multi-source project workflows. |
| `project_selection` | `selection.cpp` | Migrate default, explicit, repeated, and invalid target choices to `--target` CLI cases. |
| `project_options` | `options.cpp` | Migrate precedence/isolation to multi-target source fixtures whose success or output depends on the selected options. |
| `project_plan` | `plan.cpp` | Migrate validation/conflicts/no-write behavior to project CLI workflows; defer checks requiring generated outputs until outputs exist. |
| `project_execution` | `execution.cpp` | Migrate callback-based execution assertions to real CLI invocations with valid/failing sources and observable target selection. Delete replaced callback bookkeeping checks. |
| `project_template` | `template.cpp` | Migrate to `--new-project`, inspect the generated manifest, then use it in a real project invocation. |
| `project_create` | `create.cpp` | Split: migrate creation/refusal/path cases and concurrent creation to separate CLI processes; retain only unavoidably injected write/close-failure cleanup cases. |
| `project_hardening` | `hardening.cpp` | Split: move corpus/native paths/aliases to real CLI workflows, with explicit capability skips; retain only impractical resource-budget injections. |

## Codegen inventory

Sources relative to `tests/compiler/codegen/`. Most replacements are **deferred** until the compiler driver exposes the backend. Current synthetic LLVM tests do not prove Erlang compilation.

| CTest name | Source | Proposed action and replacement |
| --- | --- | --- |
| `codegen_sdk` | `sdk.cpp` | Delete after coverage review against configure/link probing and `codegen_dependency`; preserve any unique SDK contract in a real build/consumer scenario. |
| `codegen_results` | `results.cpp` | Defer/split: compile successful/failing batches and verify artifacts, diagnostics and no partial output; retain essential ownership constraints and otherwise unreachable failure transitions only. |
| `codegen_ownership` | `ownership.cpp` | Defer/split: multi-module compilation and retained results under sanitizers; keep narrow LLVM callback/lifetime injection cases if no real compilation triggers them. |
| `codegen_target` | `target.cpp` | Defer: compile fixtures for supported targets, inspect object architecture, and check invalid-target diagnostics. Cross-target inspection must not be reported as execution. |
| `codegen_verification` | `verification.cpp` | Split/defer: validate emitted IR in compilation workflows; retain a minimal deliberately-invalid-IR injection test because normal source should never generate corrupt IR. |
| `codegen_emission` | `emission.cpp` | Defer: CLI emission of IR/bitcode/object, independent artifact inspection, native linking and execution; retain only unreachable output-failure injection. |
| `codegen_term_abi` | `term_abi.cpp` | Defer/split: emitted values round-trip through the linked runtime; preserve cross-width boundary checks until equivalent 32/64-bit coverage exists. |
| `codegen_features` | `features.cpp` | Defer/split: unsupported real Erlang constructs must fail through CLI with useful diagnostics and no output. Preserve sink-failure injection if still relevant. |
| `codegen_runtime_terms` | `runtime_terms.cpp` | Treat as compiler/runtime integration coverage; migrate synthetic values to compiled Erlang fixtures when lowering exists, preserving compiler/runtime agreement checks. |

## Runtime and ABI inventory

Sources relative to `tests/`. A separately built C++ consumer of the actual runtime is a valid integration destination while Erlang execution is unavailable. Scenarios should combine lifecycle, terms, dispatch and teardown and check public outcomes, rather than merely relocating internal assertions.

| CTest name | Source | Proposed action and replacement |
| --- | --- | --- |
| `runtime_term_tag` | `runtime/term_tag.cpp` | Split: cover accepted/rejected words through real runtime/ABI consumers; retain a compact encoding truth-table exception for reserved bit patterns unreachable from valid programs. |
| `runtime_term_layout` | `runtime/term_layout.cpp` | Replace runnable smoke test with compile-time layout/header validation where needed. Delete runtime registration once required assertions are compiled by that validation; avoid freezing unused sketch layouts. |
| `runtime_features` | `runtime/features.cpp` | Migrate observable errors to runtime consumer/output scenarios; remove duplicated catalog assertions; retain only necessary sink-failure injection. |
| `runtime_lifecycle` | `runtime/lifecycle.cpp` | Consolidate into real consumer workflows covering startup, contexts, isolation, busy shutdown and teardown; preserve invalid-option coverage at the supported API boundary. |
| `runtime_lifecycle_failure` | `runtime/lifecycle_failure.cpp` | Keep temporarily as a justified fault-injection exception: deterministic allocation failures and rollback are not reliably reproduced by ordinary end-to-end runs. Isolate cases in processes if needed; do not delete unique failure-path coverage. |
| `runtime_memory` | `runtime/memory.cpp` | Migrate immediate copies/context lifetimes into linked consumer scenarios; defer actual allocation/GC program tests until implemented. Preserve rejection behavior meanwhile. |
| `runtime_scheduler` | `runtime/scheduler.cpp` | Consolidate current bookkeeping behavior into lifecycle/dispatch consumer scenarios; defer actual process scheduling/message workloads until execution exists. Retain only unreachable invalid-state injection. |
| `runtime_immediate` | `runtime/immediate.cpp` | Consolidate valid values into consumer dispatch/round trips; retain small malformed-word/boundary cases that cannot arise from valid Erlang. |
| `runtime_builtins` | `runtime/builtins.cpp` | Migrate to linked consumer module registration/publication/invocation/pinning/teardown workflows; later invoke through compiled Erlang. |
| `runtime_builtin_bridge` | `runtime/builtin_bridge.cpp` | Keep/consolidate as real ABI/runtime integration; use consumer calls and process-output checks. Retain invalid pointers/counts only as safe boundary validation, not undefined-behavior probes. |
| `runtime_services` | `runtime/services.cpp` | Consolidate service refusal, state preservation and once-only reporting in runtime consumer/output scenarios; retain sink and lifetime fault cases until equivalent coverage exists. |
| `abi_integers` | `abi/integers.cpp` | Split: migrate native encode/decode agreement to compiler/runtime consumer round trips; retain compact mathematical 32/64-bit overflow/boundary checks until native target coverage replaces them. |
| `abi_features` | `abi/features.cpp` | Consolidate stable diagnostic spelling/escaping into output fixtures. Delete assertions coupling catalog metadata to test names/plan steps after review; retain any required stable ID compatibility snapshot in one place. |

## Existing broader tests to preserve and extend

These are not blanket unit-test deletion candidates:

- CLI workflows: `cli`, `project_cli`, `project_workflow`.
- Scanner/preprocessor stage fixtures: `lexer_golden`, `scanner_oracle`, `preprocessor_golden`, `preprocessor_oracle`.
- Parser stage fixtures: `parser_phase1_golden`/`parser_phase1_oracle` through `parser_phase6_golden`/`parser_phase6_oracle`, plus `parser_historical_golden`/`parser_historical_oracle`.
- Source/corpus/reference checks: `parser_coverage`, `parser_corpus`, `parser_reference`, `parser_oracle`, `otp_oracle`. Some establish OTP/reference expectations rather than comparing the product directly; pair them with actual compiler runs before claiming end-to-end coverage.
- Build/link integration: `codegen_dependency`, `runtime_generated_link` (`runtime/link.cmake` and `runtime/link_consumer.cpp`).
- Subprocess diagnostic checks: `codegen_feature_output`, `runtime_feature_output`, `runtime_lifecycle_output`, `runtime_builtin_output`, `runtime_service_output`. Preserve their driver modes while consolidating C++ test executables; they depend on those executables.
- Helper executables `lexer_dump` (`tests/compiler/lexer_dump.cpp`), `preprocessor_dump` (`tests/compiler/preprocessor/dump.cpp`), and `parser_dump` (`tests/compiler/parser/dump.cpp`) are used by fixtures, not independently registered unit suites. Remove only after every dependent fixture uses a replacement interface.

## Execution order and completion criteria

1. [x] Record the configured CTest inventory and baseline on a working host with `BUILD_TESTING=ON`, compiler/runtime enabled and working OTP 29+. Separate failing, skipped and passing tests; a configure pass is not a test pass.
2. [x] Migrate project tests first into existing CLI/workflow suites. For each old assertion, record its new fixture or the reason it is redundant/unnecessary before deletion.
3. [x] Migrate frontend source-level cases into CLI/golden/OTP fixtures. Preserve negative diagnostics, recovery, limits, timeout and Unicode cases; do not replace semantic assertions with exit-code-only smoke tests.
4. [x] Consolidate runtime consumers and diagnostic subprocess tests. Preserve failure rollback and ABI boundary exceptions with short documented reasons.
5. [ ] **Deferred until lowering/driver integration.** Perform codegen/compiled-program migrations when driver integration and lowering support each scenario. Link and run native artifacts, comparing supported behavior with OTP; inspect cross-target artifacts separately.
6. [x] Remove obsolete CTest registrations, executables, sources and unused helpers together. Keep distinct regressions even when merging suites, and avoid new product flags solely to expose internals for tests.
7. [ ] **Runs performed; remaining blockers documented in the ledger.** Build/run affected replacements, then relevant broader regressions. Exercise Debug/Release and supported host capabilities as relevant; use sanitizer runs for lifetime/stress replacements. Follow AGENTS.md's fresh full `check-quality` requirement before a clean commit.
8. [x] Update this inventory, `.agents/files.md`, and any feature catalog references to renamed/deleted tests. Mark migration complete only when useful behavior is covered and remaining unit exceptions are explicitly justified.

No test-count target: success means fewer implementation-coupled tests and stronger observable behavior coverage, not simply fewer CTest entries.
