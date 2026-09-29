# Test migration coverage ledger

The [migration strategy](../.agents/00-finished.md#testing-strategy-and-migration)
is archived with completed work. Replacements use the production CLI unless an API
boundary cannot be reached through it. A subprocess around the old unit
executable does not count as migration. Backend compilation remains deferred.

## Baseline (Windows x64, 2026-09-28)

Full Debug configure/build, compiler and runtime enabled, OTP 29.1.1:
93 registered, 78 passed, 15 failed, 0 skipped. Commands and machine-readable
inventory/results are retained in ignored `build/test-migration/` as
`baseline.cmd`, `baseline-full.log`, `baseline-inventory.json`, `baseline.xml`.

Failures: parser_coverage, parser_corpus (reference checkout revision);
parser_hardening (stack crash); parser_phase2/4/5_golden and corresponding oracle
suites, lexer_golden, preprocessor_golden/oracle, parser_reference (fixture
comparisons); project_create; project_workflow (Unicode argv). These are baseline
failures, not migration passes. Byte-sensitive fixtures now disable Git text
conversion. The CLI embeds a Windows UTF-8 manifest. Creation rejects an existing
dangling symlink before opening its destination, as the old creation test required.

## Project assertion review

The replacement `project_cli` and `project_workflow` pass on this host before
removing the old normal-behavior suites. Their scripts include the case files
named below; all cases check exit status and diagnostics/output, with bounded
subprocess execution.

| Old suite / case group | Replacement or retained exception |
| --- | --- |
| dependency: TOML accept/reject | `project_cli`: valid project plus `manifest_cases.cmake` syntax and duplicate keys. No separate toml++ behavior test needed. |
| model: defaults, independent options, moved ownership | `selection_cases.cmake` template defaults and execution order, existing multi-target values. STL move/copy shape assertions removed; compiler-owned values are observed after the actual loader returns. |
| loader: syntax and coordinates, Unicode files, absent/directory | `manifest_cases.cmake` syntax/duplicate_key/coordinates, existing CLI missing/directory checks and Unicode workflow. Tiny byte boundary and injected read failure retained in `project_limits`. |
| decode: all schema rejection rows | Individually named `manifest_cases.cmake` cases preserve every normal rejection. Missing-source coordinates test the owned source site at line 4. Target/entry budget injections retained in `project_limits`. |
| decode_options: values and nested errors | All eight rejection inputs in `manifest_cases.cmake`; `options_cases.cmake` observes macro literals, include/application paths and feature values through real processing. |
| paths: fallback order, primary/absolute paths, invalid first candidate, normalized directory | `discovery_cases.cmake`: fallback/primary/absolute/blocked_fallback/normalized_directory; linked_manifest_base covers a symlinked manifest directory. CLI processes cannot mutate the caller's cwd. |
| glob: wildcard boundaries, Unicode, hidden files, case, malformed grammar | `discovery_cases.cmake`: direct/recursive/double_recursive/one_scalar/star_then_scalar/case_sensitive_pattern/bad_glob. Invalid UTF-8 and injected work limits retained in `project_limits`. |
| discovery: sorted traversal, empty/missing directories, no match, directory cycles, file/dangling links, special base | `discovery_cases.cmake`: exact module order and each explicit capability case; entry/depth/work budgets retained in `project_limits`. |
| sources: first occurrence order, overlap, search-only exclusion, links/case, per-target repeat, empty selection | discovery overlap/alias/empty cases, original workflow's intentionally invalid unselected source, 258-module two-target corpus. |
| selection: default/explicit/repeated order, invalid names without discovery | Existing `project_cli` print_all/print_reversed/repeated-target/unknown_target, workflow unselected_missing. |
| options: includes, apps, defines, features, isolation, invalid settings | `options_cases.cmake` decoded_options/option_overrides/source_roots_are_not_headers/invalid_literal/unknown_feature; existing duplicate macro and target-isolation cases. |
| plan: selection before discovery, output conflicts, frontend ignores outputs, aliases, no writes | Existing workflow preflight/unselected_missing/check_preserves; selection_cases output_collision/frontend_ignores_outputs/single_output_override; discovery output_identity_collision. Exact unpublished internal output paths deferred until artifacts exist. |
| execution: callback order, failure latch, target context, both modes | selection_cases execution_order/execution_failure/execution_default_failure/execution_check_failure and existing successful default/check runs. Callback bookkeeping is replaced by real sources and emitted values. |
| template: deterministic annotated schema, defaults, platform output | selection_cases template_second/template_defaults and byte equality, annotations, native output suffix; populated template is processed by CLI. Other platform spelling is exercised on that platform. |
| create: filenames, no-overwrite, directory/missing parent, Unicode, dangling link, race | selection_cases new_*; options_cases separate concurrent creators, native_created_manifest and dangling link. Write/close cleanup remains `project_creation_failure`. |
| hardening: 129-file two-target corpus, repeatability, Unicode/semicolon/base punctuation, aliases | discovery corpus, native creation and existing workflow; resource injections remain `project_limits`. UNC/drive-relative path syntax remains a small exception without requiring a network share. |

Remaining exceptions must keep their own purpose comments. Capability skips are
reported for each unavailable filesystem operation, never substituted for an
entire test pass. Failure injection and cross-width ABI checks remain until a
real workflow provides equivalent behavior coverage.

## Frontend assertion review

`frontend_cli` compares complete AST/stdout, stderr and status for each real
source in `tests/fixtures/parser/cli/`; `.args` selects nondefault CLI options.
The existing golden and live OTP suites remain registered. Source snapshots test
syntax and printing, not execution semantics that the compiler does not implement.

| Old suite / case group | Replacement or retained exception |
| --- | --- |
| parser_probe: valid/rejected directives, trailing input | `probe_*` CLI fixtures. The production probe API remains in use and is not removed. |
| lexer: decoded token families, invalid literals, locations/encoding | Existing scanner golden/OTP suites plus `literal_*`, UTF-8/Latin-1/logical-CRLF and feature keyword CLI fixtures. Physical byte offsets and encoding-error offsets remain in `parser_tokens`. |
| printing: token spacing, sigils, options, Latin-1 | `printing_roundtrip`: product `--print-pp`, then normalized token comparison after reprocessing. Raw/custom/triple sigils and macro/feature options have real files. |
| printing_ast: exhaustive value/tree rendering and depth | `tree*` exact output fixtures and 9,000-operator output depth/tail check in `frontend_cases`; `parser_stress` adds 12,000-operator parse/print/normalization. |
| preprocessor_forms: malformed envelopes, recovery, passthrough | `directive_*` fixtures retain every malformed case and exact errors; workflow truncations run twice with bounded output. Raw unexpanded events, EOF/session isolation and the raw reader's misplaced-directive policy remain API exceptions in `forms.cpp`. The full preprocessor intentionally has different passthrough behavior. |
| preprocessor_semantics: macros, conditions, encoding, includes, features | `semantics.tsv` maps source fixtures to old functions; `preprocessor_workflow` adds real include trees, options/order/environment, invalid and Latin-1 includes, 13 evaluated condition cases, macro chains and nesting. Tiny expression/expansion/include budgets and throwing inactive-reader injection remain in `semantics.cpp`. |
| parser_tokens: diagnostic forwarding, source transport | Macro/include diagnostic fixtures and existing stage oracles cover visible output. Cursor exhaustion, token ownership/EOF anchors, byte offsets and transport invariants remain. |
| parser_forms: accepted forms and recovery | `forms_*` exact CLI trees/errors. Private form category/rollback and invalid-handle cases remain. |
| parser_expressions: literals, aggregates, operators and precedence | `expressions_*`, `tree*` and existing OTP projections. Injected node/depth/work budgets, private category/handle invariants remain. Source stress moved to `parser_stress`. |
| parser_clauses: shapes, patterns/guards, all negative rows | `clauses_*` CLI snapshots preserve recovery and rejection. Builder rejection/rollback and synthetic provenance remain. |
| parser_control / parser_exceptions | `control_*` / `exceptions_*` syntax, feature snapshots, malformed clauses and recovery. Private invalid-child/ID/rollback and injected limits remain; block depth is also exercised through the CLI. |
| parser_comprehensions / parser_structural / parser_binaries | Corresponding source snapshots plus wide qualifier, list, record/map postfix, segment/modifier and nested-binary CLI stress. Builder category/child/rollback and tiny budget invariants remain. |
| parser_types / parser_specifications | Corresponding source snapshots and OTP phases preserve accepted/rejected syntax and following-form recovery. Invalid/foreign/stale type/spec handles and private rollback invariants remain. |
| parser_hardening | `parser_stress` covers large normalization, precedence chains, recursive/wide syntax and bounded failure. Keep explicit synthetic EOF anchors, tiny printer/work/diagnostic limits and the public API's raised nesting hard ceiling; the latter still exposes a baseline Windows stack overflow. |
| parser_mutations | `mutations.json` preserves five token seeds, mt19937 seed 0x29a016 and all 900 delete/duplicate/swap operations. `mutations.cmake` renders real source, runs the CLI twice, checks full AST/diagnostic repeatability and a surviving following form, with per-process and suite timeouts. |
| parser_consumer | Independently configure/build a public-header consumer and verify owned syntax after source/preprocessor destruction. Compile as C++23 using the parent's compiler/CRT/flags. |
| parser_ast / parser_attributes | Keep current growth/lifetime/provenance/rollback and raw attribute/doc-file ownership checks. CLI processing expands external docs, so it cannot replace the raw parser contract. Do not retire remaining growth/lifetime assertions until a compatible frontend sanitizer run demonstrates equivalent coverage. |

## Runtime, ABI and backend assertion review

| Old suite / case group | Replacement or retained exception |
| --- | --- |
| runtime_lifecycle and silent output | Removed after `runtime_generated_link` combines startup rejection, two-runtime/context isolation, busy shutdown, identity nonreuse, expired lifetime tokens and RAII cleanup with actual dispatch/copy/publication. Consumer stdout/stderr must both be empty. |
| runtime_memory: valid copies and context ownership | Linked consumer copies signed boundary integers and empty containers without allocation, retains results beyond source runtime teardown and repeats the complete workflow 32 times. Malformed words, budgets, arithmetic overflow, invalid slots and unavailable heap allocation/GC stay in `runtime_memory`. |
| runtime_scheduler: normal waiting/exit/shutdown | Linked consumer calls builtins inside dispatch boundaries, returns to waiting/runnable states, exits, stops admission and tears down contexts. Invalid/stale/foreign states, growth and synthetic destructor ordering stay in `runtime_scheduler`. No actual worker/message execution is claimed. |
| runtime_immediate | Native valid values round-trip through the consumer and builtin bridge. Overflow, reserved bits and hostile words remain a small focused exception. |
| runtime_builtins | Consumer covers copied callbacks, arity distinctions, registry rejection/freeze, publication, missing exports, native calls and retained callable/code pins after owner teardown. Malformed arguments, callback exceptions, failure reporting and synthetic capture/image destructor ordering remain. |
| runtime_builtin_bridge / runtime_services | Preserve real API integration and subprocess output checks, including safe invalid pointer/count boundaries, state preservation, once-only delivery and lifetime/sink faults. Wrapping them again adds no coverage. |
| runtime_features / abi_features | Exact stderr fixtures cover complete/partial/escaped context and reserved atom-collection reporting; existing output modes cover silence/once-only delivery. Keep sink refusal/throw and invalid-ID injection, plus one stable ID/name snapshot. Delete catalog-to-test-name/plan-step coupling. |
| runtime_term_layout | Private static assertions now compile in the build-only `runtime_term_layout_tests` object library; runnable registration removed. |
| runtime_lifecycle_failure / runtime_term_tag / abi_integers | Retain allocation rollback/failure sweeps, reserved low-tag truth table and mathematical 32/64-bit overflow boundaries. Real native execution cannot reproduce all these inputs or substitute for both word widths. |
| codegen_sdk | Remove duplicate executable/CTest. `codegen_dependency` now builds **and executes** its separate LLVM consumer; configuration retains the SDK compile/link/version probe. |
| codegen_results / ownership / target / verification / emission / term_abi / features / runtime_terms | Defer source-driven migration until lowering/artifact emission is reachable through the CLI. Preserve invalid IR, rollback, target widths, diagnostic sink failures and compiler/runtime word agreement. `.agents/04-compile.md` routes steps 15–46 to their eventual replacements. |

The plan's baseline inventory remains a record of old names, not a list of current
registrations. Removed suites have their registrations, sources and unused helpers
removed together. No extra product flag exposes internals for testing.

## Final validation (2026-09-28)

Windows x64, clang-cl/LLVM SDK 23.1.2, MSVC 14.50 STL and `/MT`, C++23,
CMake 3.29, Ninja, Boost 1.90.0, toml++ 3.4.0, OTP 29.1.1.
Use `C:/Program Files/Erlang OTP/erts-17.1/bin/escript.exe`; the generic `bin`
wrapper is broken on this host. Both compiler and runtime were enabled in the
fresh Debug quality configuration and the separate Release build.

| Run | Passed | Failed | Skipped | Evidence under `build/test-migration/` |
| --- | ---: | ---: | ---: | --- |
| Original full Debug baseline | 78 | 15 | 0 | `baseline-full.log`, `baseline.xml`, `baseline-inventory.json` |
| Final full Debug | 74 | 1 | 0 | `final-debug.log`, `final-debug.xml`, `final-inventory.json` |
| Final full Release | 74 | 1 | 0 | `final-release.log`, `final-release.xml` |
| Runtime-only Debug ASan | 15 | 0 | 0 | `asan-runtime.log`, `asan-runtime.xml` |

The final helper simplifications were then rebuilt and the affected tests rerun:
four each in Debug/Release and two under runtime ASan, all passing
(`final-affected.log`). There are 139 exact CLI source fixtures plus the seeded
900-case mutation corpus. Runtime consumers link independently and execute;
no generated Erlang execution is claimed. Hard links, file/directory symlinks,
dangling links, symlinked manifest bases and case aliases were all available and
exercised. No filesystem capability cases were skipped.

During the migration, `references/otp` was left unchanged. An isolated worktree at
`build/test-migration/otp` uses commit
`751f87b703fe5948607d08e82599ce644b772e76`. Its own Perl `beam_makeops -compiler`
generated `lib/compiler/src/beam_opcodes.hrl`; LF bytes match the already pinned
SHA256. `ERLANG_AOT_OTP_SOURCE_ROOT` pointed the migration builds there. Both corpus and
344-production grammar audits pass. The grammar audit excludes the new `cli/`
product-only malformed inputs, retaining its original checksum/witness contract;
those inputs have their own CLI snapshots. They must not silently change pinned
grammar witnesses or be treated as valid OTP preprocessor programs.

Remaining blockers are explicit:

- `parser_hardening` still crashes on Windows in the API-raised nesting ceiling
  case (hard maximum 512), in both Debug and Release. This existed in the baseline;
  the test remains enabled. Default-limit CLI stress and all 900 mutation cases pass.
- Required `cmake --build build/debug --target check-quality` ran after a fresh
  full configuration. Lizard passed. clang-tidy 22.1.8 failed on existing Windows
  exception-escape findings in `Lexer`, `IncludeFrame`, `PreprocessorOptions`,
  preprocessor `File`, `PlannedTarget` and compiler `main`, plus Boost cpp_int
  leak/bounds analyzer findings. See `quality.log`. No checks or thresholds changed.
- Focused Lizard passes all 25 changed/new C++ test files. Focused clang-tidy
  identifies further retained-test findings (uncaught main exceptions, unchecked
  optionals and empty catches); it is not a passing gate. The new project exception
  suites and runtime consumer pass focused tidy; final runtime diagnostic changes
  also pass. New helper cognitive-complexity findings were fixed by splitting
  responsibilities, not suppressing checks. See `test-{lizard,tidy}.log` and
  `final-affected.log`.
- Full compiler ASan cannot link the installed LLVM SDK: its rpmalloc object and
  ASan static-runtime thunk define duplicate CRT allocator symbols. See `asan.log`
  and `asan/CMakeFiles/CMakeConfigureLog.yaml`. Runtime-only ASan works with
  `/EHsc /fsanitize=address`, `/MT`, installed ASan dynamic library/static thunk,
  and matching runtime DLL on PATH. STL string/vector container annotations were
  disabled consistently; this is address instrumentation evidence, not container
  bound, leak, UBSan or frontend sanitizer evidence. Frontend ownership checks
  remain until a compatible full sanitizer toolchain validates replacements.

Reproduction scripts `debug.cmd`, `final-{debug,release}.cmd`,
`asan-runtime.cmd` and `final-affected.cmd` retain the exact host flags and commands.
All changed C++ files are clang-formatted; `git diff --check` passes. Logs/builds
stay ignored. No clean-commit gate is claimed. Linux, macOS revalidation of this
migration and native 32-bit runs remain pending; earlier host results are historical.

The subsequent source-reference refresh follows `maint-29`; see
[the current pin and refresh policy](otp-reference.md). The active Debug build
now uses `references/otp`; the migration worktree remains historical evidence.

## Windows gate repair (2026-09-28)

The subsequent compiler-plan prerequisite run passes all 75 Debug CTests and the
full Lizard/clang-tidy gate. An 8 MiB executable stack resolves the retained raised-depth
parser test. Move-safe frontend storage and isolated numeric/CLI boundaries resolve
the earlier quality findings without disabling checks. Real preprocessor CLI cases
cover binary64 truncation through the largest finite value and large signed decimal
output. Earlier failing runs above remain historical evidence. Other native hosts
and full frontend sanitizers remain pending.


Compiler steps 18–20 keep source binding/call diagnostics in `frontend_cli`, including
project batches and target isolation. `semantic_types` is a temporary focused
exception for type identity ownership, lattice/widening and structural distinctions
without public inspection until step 39. It parses actual Erlang type declarations;
it does not add a product testing switch. Step 20's full Windows Debug gate passes
77/77 tests and full Lizard/clang-tidy.

Step 21 extends the same real CLI workflows with declared type/spec diagnostics,
remote visibility, include provenance and project isolation. `semantic_declared_types`
is a second temporary invariant exception for parameter substitution, opaque/nominal
barriers, quoted-name identity and bounded graph ownership. No inference or emitted
Erlang execution coverage is claimed. The final fresh full gate passes 78/78 CTests,
Lizard and clang-tidy on Windows x64; other native hosts remain pending.

Compiler steps 24–27 add `codegen_lowering`, a temporary real-source stage adapter
that parses fixture files, runs semantic/type analysis, verifies LLVM modules and
inspects emitted objects. It covers literal limits, parameter projections, ordered
local calls and separate remote definition/import symbols. Frontend CLI cases retain
invalid-call, export, cycle and batch-isolation diagnostics. This does not claim
CLI artifact publication or execution; no synthetic failure/lifetime tests are retired.
Final fresh Windows x64 Debug validation passes 80/80 CTests and full Lizard/clang-tidy.
Other native hosts and frontend sanitizers remain pending.

Step 40 (2026-09-29): Public CLI objects execute in a separately configured Clang harness through the mandatory runtime link target at O0/O2; integer/immediate boundaries, projection and nested calls, ABI rejection, missing-runtime failure and explicit teardown pass. Fresh Windows x64 Debug compiler/runtime build: 95/95 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.

`codegen_differential` adds 150 fixed/seeded calls through CLI-emitted native objects
and the installed OTP compiler/runtime. Annotated/unannotated equivalents, a false
return specification, nested argument order and repeatability run at O0/O2 with
specialization enabled/disabled. `frontend_cli` retains unsupported source fixtures.
The new workflow does not replace private rollback/invalid-IR tests or mathematical
32-bit term tests; these exercise states the supported source/native host cannot reach.

Step 41 (2026-09-29): 150 seeded/fixed calls agree with OTP and an independent evaluator across four optimization/specialization modes, repeated twice; annotated/unannotated pairs and incorrect specs preserve behavior. CRLF and CMake native-path issues in the new test were fixed before the passing gate. Fresh Windows x64 Debug compiler/runtime build: 96/96 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.

Step 42 (2026-09-29): Cost records cover source and synthetic guards at O0/O2 with specialization disabled/enabled. High-arity wide-union inputs remain generic, O2 outputs match byte-for-byte, 3/32/128 caps and 2x growth hold, and native guard/fallback results agree. Timings are descriptive only. Fresh Windows x64 Debug compiler/runtime build: 97/97 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.

Step 43 (2026-09-29): CLI-emitted objects pass SDK readobj/nm inspection for seven ELF, Mach-O and COFF targets at O0/O2, including architecture, exports/imports, runtime references, ABI widths/tags, exact integer endpoints and failure without publication. Foreign native execution remains pending. Fresh Windows x64 Debug compiler/runtime build: 98/98 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.

Step 44 (2026-09-29): All compiler catalog families are audited through both CLI modes at O0/O2 with verbosity on/off. Explicit executable output now fails instead of silently succeeding. Native allocation rejection preserves generated calls, heap accounting and clean teardown; atom collection is documented as a reservation without an owner. Fresh Windows x64 Debug compiler/runtime build: 99/99 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.
