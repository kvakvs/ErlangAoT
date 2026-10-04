# Completed implementation work

Compact implementation record, consolidated 2026-09-30; pattern/guard work added
2026-10-03. This archive preserves completed work, compatibility decisions,
historical evidence and open obligations. Current behavior lives in the linked
documentation; current architecture and file ownership are in
[arch.md](arch.md) and [files.md](files.md).

| Area | Delivered status | Remaining qualification |
| --- | --- | --- |
| Foundations | CLI/CMake scaffold, 2026-09-17 | Native objects implemented; executable startup and positional linking arrived in plan 11 steps 5–6; project-target linking remains. |
| Preprocessor | Steps 1–13 implemented, 2026-09-18 | Focused OTP comparisons; no full upstream Common Test run. |
| Parser | Steps 1–17 complete; step 18 implementation and macOS validation, 2026-09-19 | Required platform matrix is not fully closed. |
| Projects | Steps 1–22 complete, 2026-09-20 | Historical host evidence and later Windows evidence remain distinct. |
| Test migration | Available frontend/project/runtime migrations implemented, 2026-09-28 | Generated-program workflows delivered; frontend sanitizers remain pending. |
| Compiler/runtime milestone | Steps 1–46 complete, 2026-09-29 | Historical immediate-only subset and runtime skeleton; later pattern/guard delivery is recorded below. |
| Pattern matching and guards | Steps 1–20 and added step 15a complete, 2026-10-01–03 | Function clauses and body matches over the admitted domain; other source contexts, GC/process owners and native platform gaps remain open. |
| Plan 11 baseline | Step 1 complete, 2026-10-03: `maint-29` unchanged; 125/125 CTests, 258 quality units, 14 OTP audits and 19 corpus checks pass | Recorded in [validation](../docs/validation.md#current-baseline). Steps 1A (fast/full tests), 1B (changed-file quality) and 1C (docs consolidated from 81 files to 15) done. Step 2: six target program fixtures with OTP goldens and a feature map. Step 3: entry/argv/exit contract (`docs/executables.md`) with validated `--entry` and manifest `entry`. Step 3A: `#!` sources compile with OTP escript rules. Step 4: `~w`/`erlang:display/1` term printing with OTP goldens and source-callable `erlang:display/1`. Step 5: startup object (`eav1_start`, `erlang_aot_main_v1`) and `erlang:halt/0,1`; manually linked programs pass argv/exit-path tests. Step 6: `erlangaot -o` links positional batches with Clang and the runtime archive (`compiler/src/linking/`), with staged publication and runtime-target checks. |

**Still unfinished:** project-target executable linking, GC and graph
copying, process execution/messaging, additional Erlang source contexts and
representations, and native platform/sanitizer closure. See the
[explicit completion checklist](#outstanding-work-to-finish).

## Foundations

The project targets Erlang/OTP 29 on Windows x86-family, Linux x86/ARM and macOS
Apple Silicon. The delivered build uses C++23, CMake 3.28+, warnings as errors,
Clang as the reference toolchain, and separate compiler/runtime enablement.
`erlang_aot` produces `erlangaot`; `erlang_runtime` is independently buildable;
`erlang_aot_abi` carries shared build contracts. The runtime does not depend on
compiler internals or LLVM. Cross builds separate the host compiler from the
target runtime, with matching generated-code/runtime ABI and toolchain settings.

`compiler/` owns driver, source/diagnostics, lexing, preprocessing, parsing,
AST, semantics and LLVM code generation. `runtime/` owns terms, allocation,
process contexts, scheduling and module dispatch; `abi/` owns shared internal
contracts. `cmake/`, `tests/`, `examples/` and `docs/` hold build rules,
verification, workflows and documentation. See the current file map for exact
implementation locations.

Stage boundaries use owned internal data. Only directory locations are reserved
for `compiler/src/stage_readers/{preprocessed,abstract,ir}/`; readers and a
public interchange format are not implemented. APIs remain project-internal
C++23; C linkage and external interoperability await a concrete need. Earlier
selectable C++26 and C-wrapper proposals are superseded. Diagnostics retain
source/macro/ include context, use stderr and report failure through exit
status.

## Preprocessor

Contract, limits and per-step evidence:
[preprocessor.md](../docs/preprocessor.md). The implementation is independent of
LLVM/runtime and processes each source in an isolated session. Boost.Parser
remains a pinned character-parser dependency; the rejected token-iterator
experiment is not a basis for another token adapter. Expanded tokens are handled
explicitly and retain owned source spellings.

| Steps | Delivered behavior |
| --- | --- |
| 1–3 | Parser/oracle boundary; source buffers, encoding and locations; Erlang lexer; form/directive dispatch, recovery and diagnostic events. |
| 4–6 | Object and parameterized macros, overloads/zero arity, source-order definitions/undefinition, recursion checks, substitution/rescanning and original-token stringification. |
| 7–9 | Nested conditionals and skipped forms; include/include_lib lookup; contextual module/file/line/function/machine/OTP macros. |
| 10–11 | Restricted preprocessing expression evaluator; feature configuration, keywords and feature-query macros. |
| 12–13 | Warning/error directives, failure latching, provenance, CLI options, live OTP/golden fixtures and copied-header smoke coverage. |

Lexing supports arbitrary decimal integers, decoded Unicode, sigils and triple
strings. Macro argument scanning balances delimiters and distinguishes fun
types/ references from blocks. Object and parameterized definitions may coexist;
stringification uses the original argument tokens. Logical `-file` context stays
separate from physical spelling. Includes preserve nested sibling precedence,
explicit application roots and documented environment handling.

Condition evaluation supports the allowed guard-expression subset, exact
integers, terms, bitstrings and exact map keys. Only `true` selects a branch;
ordinary arithmetic/evaluation failure selects false, while malformed syntax and
resource failures diagnose. It never executes arbitrary Erlang source.
Conditional structure is still checked inside skipped regions. Contextual
function macros recognize headers without embedding a full parser in
preprocessing.

Production feature metadata distinguishes approved/default-enabled `maybe_expr`
from experimental/default-disabled `compr_assign`; OTP synthetic test features
are excluded. Expansion, include nesting, token production, expression depth and
integer/bitstring size are bounded; defaults and diagnostic policies are in the
contract. Full parsing, binding, record expansion, transforms and external
documentation ingestion belong to later stages or remain outside this scope.

## OTP preprocessor test reference

The initial inspection on 2026-09-18 used OTP-29.1 commit
`751f87b703fe5948607d08e82599ce644b772e76` (the annotated tag object is
different). It inspected source and selected fixtures; the checkout was not
built and the full upstream suites were not executed. Current research tracks
`maint-29`, recorded as `21776803ecd11f5fa948732c0ec66b8f325dedfc`. Follow
[otp-reference.md](../docs/otp-reference.md) before future OTP-dependent work:
refresh upstream, checkout/pin, corpus hashes, grammar evidence and current docs
together; retain historical revisions and never refresh silently during tests.
`references/otp` is ignored research material, not a submodule or normal build
input.

Primary source: `lib/stdlib/src/epp.erl`; primary suite:
`lib/stdlib/test/epp_SUITE.erl`. Many useful cases are embedded Erlang strings,
not standalone data files.

| Behavior | Useful epp suite cases |
| --- | --- |
| Definitions/predefined names | `predef_mac`, `upcase_mac_1`, `upcase_mac_2`, `variable_1`, `otp_8130` |
| Recursion | `rec_1`, `not_circular`, `otp_11728`; data `mac.erl`, `mac2.erl`, `mac3.erl` |
| Arguments and overloads | `overload_mac`, `otp_8388`, `otp_8130`, `fun_type_arg` |
| Stringification | `otp_7702`, `otp_8130`, `stringify`, `not_circular` |
| Conditional structure/evaluation | `ifdef` helper in `otp_8130`, `otp_16824`, `test_if` |
| Includes | `include_local`, `otp_8130`, `otp_8911`, `otp_10820` |
| File/source context | `otp_5362`, `otp_7702`, `file_macro`, `source_name`, `deterministic_include`, `nondeterministic_include` |
| Function context | `function_macro` |
| Encoding | `encoding`, `otp_10302`, `otp_14285` |
| Diagnostics and scanning | `test_error`, `test_warning`, `scan_file`, `gh_8268` |

Other reference anchors:

- `erts/test/erlc_SUITE.erl`: `features_directives`, `features_macros`,
  `features_include`, `features_disable`, `features_all`,
  `features_erlc_unknown`, `features_atom_warnings`; data `f_macros.erl`,
  `f_directives.erl`, `f_disable.erl`, `f_include_1.erl`–`f_include_3.erl`,
  `f_include_exp2.erl`. `OTP_TEST_FEATURES=true` enables synthetic features, not
  product defaults.
- `lib/compiler/test/compile_SUITE.erl`: `cond_and_ifdef` with `simple.erl`,
  `makedep`, `listings`, `deterministic_include`. Dependency/listing formats are
  separate compiler behavior, not preprocessing token compatibility.
- `lib/stdlib/test/erl_scan_SUITE.erl`: `sigil_string`, `triple_quoted_string`,
  `otp_7810`; implementations `erl_scan.erl` and `erl_features.erl`.

Preserve OTP details: non-boolean `42` is false in conditions; fun types need no
`end`; duplicate macro parameters and nonparameter `??B` follow OTP behavior;
macro-generated function heads matter; feature-enabled `maybe`/`else` alter
token classification. Compare preprocessing with `epp:scan_erl_form`, parsing
with `erl_parse`, and compilation/execution separately. Normalize fixture roots
and binary64 float bits; implicit file attributes have separate native checks.

Copied `otp_assert.hrl`/`otp_file.hrl` fixtures retain upstream Apache-2.0
notices and provenance. Keep upstream-crash exceptions explicit (including
circular object macros) and native diagnostics covered. Full upstream execution
requires a matching built OTP and its documented Common Test hooks; bare
`ct_run` against an unrelated installation does not establish compatibility.

## Parser

Contracts and historical evidence: [parser.md](../docs/parser.md) and
[parser evidence](../docs/parser.md#compatibility-evidence). The parser consumes
existing expanded ordinary-form tokens directly: no printing/relexing or second
lexer. Small cursor/delimiter/operator mechanisms are shared; the preprocessor's
restricted evaluator is not generalized into the full parser.

| Steps | Delivered behavior |
| --- | --- |
| 1–4 | Pinned grammar/oracle inventory, shared token mechanics, AST ownership/provenance and transactional form parsing. |
| 5–7 | Literals/aggregates, operators/calls/remotes, function clauses, pattern syntax and guards. |
| 8–9 | Maps, unresolved records and binary/bitstring syntax. |
| 10–12 | Blocks, branching, receive, funs, try/catch, maybe and strict/zipped comprehension forms. |
| 13–15 | Ordinary/documentation/record attributes, type/opaque/nominal declarations, specs/callbacks and constraints. |
| 16–18 | Bounded diagnostics/recovery, parse-check integration, structural differential coverage, pinned corpus and available host validation. |

Move-only module-owned arenas provide distinct checked expression,
pattern-syntax, type and form IDs, typed payloads, exact integers and explicit
optional/nonempty constructs. IDs survive arena growth; borrowed references do
not. Source metadata retains logical anchors, expanded extents and physical
spelling/origin tables through macro/include boundaries without inventing
contiguous cross-file ranges. Completed ASTs remain readable after preprocessing
sessions/source managers die.

Parsing is transactional per form: failure rolls back ordinary nodes,
diagnostics mark the module failed and later valid forms can survive. Node
count, depth, work, diagnostics and printer traversal are bounded. Completed
trees are immutable and retain records/defaults/types/features without
desugaring. Documentation-file paths are data, not reads; parse-transform
attributes do not execute transforms.

Syntax acceptance is separate from binding, guard legality, linting and type
checking. Restricted `pat_expr` applies to function/fun heads; other pattern
candidates can remain permissive. Guards preserve comma/semicolon grouping and
parse general expressions. `spec`, `callback` and `record` are contextual
prefixes. OTP parser-builder normalization belongs here; record expansion and
`v3_core` do not. Parsing a feature's syntax does not enable it.

CLI modes `--parse-check`, `--print-pp` and `--print-ast` are delivered; the
current printer contract is documented separately and is not a public
interchange format. Structural oracle projections fail on unmapped nodes,
normalize only documented OTP/grouping differences, and retain native
provenance/ownership invariants.

Historical closure: 344 ordinary grammar productions witnessed, 79 SSA
exclusions, 423 total inventory rows; 43 positive authored fixtures, 183
ordinary negatives, three OTP builder exceptions (`record_helper`,
`record_extra`, `any_first`) and original `bad.erl` coverage. The ten-source
pinned corpus covers `lists`, `maps`, `sets`, `erl_scan`, `beam_ssa`, `beam_asm`
and the four headers `beam_ssa.hrl`, `beam_asm.hrl`, `beam_opcodes.hrl`,
`beam_types.hrl`; hashes are in `tests/fixtures/parser/phase6/otp.tsv`. This
proves syntax coverage for that corpus, not semantic acceptance or a full
upstream Common Test run.

## Projects

All 22 steps delivered TOML projects, target selection and safe manifest
creation. Current usage/schema: [projects.md](../docs/projects.md); historical
matrix: [validation](../docs/validation.md); example:
[project.toml](../examples/project/project.toml). Private project code owns
toml++ 3.4.0 discovery, model/loading/schema, paths/globs/discovery,
selection/options, planning/execution, CLI and creation; driver hooks share the
per-file frontend. Runtime-only builds do not discover TOML dependencies.

| Steps | Delivered behavior |
| --- | --- |
| 1–6 | Format contract, private dependency, owned located model, bounded loading, strict declarations and typed frontend options. |
| 7–10 | Path bases/fallback, bounded Unicode globs/traversal, deterministic source assembly and physical deduplication. |
| 11–14 | Ordered selection, effective options, full selected-target preflight and reusable frontend request. |
| 15–19 | Target execution, project/target CLI, annotated template, exclusive creation and creation CLI. |
| 20–22 | Real workflows, resource/native-path portability coverage and published examples. |

Version 1 requires integer `schema_version = 1`, nonempty targets, unique
case-sensitive ASCII names matching `[A-Za-z0-9_][A-Za-z0-9_.-]*`, and a
nonempty `sources` or `source_dirs` selection. Unknown keys, wrong types and
duplicates are errors throughout the manifest, including unselected targets.
Defines reuse the preprocessor's Erlang literal parser (`NAME` means `true`);
duplicates and conflicting feature lists fail. Arbitrary shell/backend options
are not accepted.

`--project` excludes positional sources; repeated `--target` selects unique
targets in first-request order, otherwise all run in manifest order. Unknown
selectors return usage status 2 and list available names. Help/version validate
syntax but do no filesystem work. Missing suffixless project operands can
resolve `.toml`. All selected targets are planned before frontend execution;
filesystem validation is limited to selected targets. Later file/target work
continues after frontend errors, with isolated settings, contextual diagnostics
and latched failure.

Manifest paths use the lexical manifest parent's base, including symlinked
manifests; CLI paths use invocation cwd. External/`..` paths are permitted, with
no shell/tilde expansion. Literal missing sources try ordered search roots;
existing invalid candidates fail. Search roots neither discover sources nor find
headers. Globs support case-sensitive `*`, Unicode-scalar `?` and
whole-component `**`; bracket/brace/negation/escape syntax is rejected. Hidden
entries participate; there are no implicit build/vendor exclusions. Directory
discovery skips symlinks, explicit directory roots may be symlinks, and selected
dangling file links fail.

Preserve source-entry order, sort each expansion by generic UTF-8 bytes, append
`source_dirs`, and deduplicate physical identity within each target, retaining
the first spelling. CLI include paths precede manifest includes (last CLI `-I`
first); CLI applications replace matching mappings and feature settings apply
last. Macro duplicates fail instead of replacing definitions.

Outputs reserve future executable paths, defaulting to `build/<target>[.exe]`.
CLI `-o` requires one compilation target and is forbidden for check/print modes;
those modes ignore manifest outputs. Preflight detects output aliases without
creating directories/artifacts. Current normal invocation compiles to verified
object buffers in memory; explicit `--emit` publishes module objects, LLVM IR or
bitcode. Explicit executable output requests fail as unimplemented; TOML output
paths remain reserved.

`--new-project` is standalone, validates native filenames/parents, appends
`.toml` unless already present case-insensitively, and creates exclusively
without replacing files/symlinks. Failed writes/closes clean up only its own
partial file. The deterministic UTF-8/LF template has one `app` target,
`sources = []`, `source_dirs = ["src"]`, empty option collections and
`build/app[.exe]` output. It creates no directories/source files. Exits: 2 for
usage/unknown selection, 1 for manifest/source/I/O failure, 0 for successful
checks/creation or warnings. Defaults/inheritance, target dependencies, imports,
profiles, exclusions, package fetching, watch/cache and parallel execution
remain outside schema version 1.

## Testing strategy and migration

The [case-level migration ledger](../docs/validation.md#test-design) is authoritative
for suite dispositions, replacements, justified exceptions and failure evidence.
Prefer production CLI workflows, real Erlang files, exact artifacts/diagnostics,
separately built runtime consumers and OTP comparisons. Test count is inventory,
not a quality target; wrapping old unit executables in subprocesses is not
migration.

The 2026-09-28 migration moved normal project behavior from 16 native suites to
CLI/workflow fixtures, moved frontend behavior to exact CLI/OTP projections and
900 bounded source mutations, and exercised runtime startup/contexts/dispatch/
copy/publication/pinning/teardown through a separately built consumer (32
repeats). Useful behavior must move before deleting a suite, its source,
registration and unused helpers. Existing broader regression/differential
fixtures remain required.

Retain API-only budgets, invalid handles, raw-stage ownership, injected
allocation/ write/close/sink failures, LLVM-invalid IR, all-64-tag truth tables
and mathematical 32/64-bit ABI boundary/layout checks when public workflows
cannot reach them. AST/raw-attribute lifetime tests await compatible frontend
sanitizer evidence. `lexer_dump`, `preprocessor_dump` and `parser_dump` are
stage adapters, not end-to-end proof; remove them only after migrating every
dependent fixture.

Keep private backend success/failure checks until equivalent real CLI artifact,
link and execution coverage exists. Steps 24–27's source-driven lowering
adapters do not satisfy that retirement condition. Do not add product flags
solely to expose internals or confuse an independent oracle with another path
through the product. Compiler steps 1–46 now supply CLI type/IR inspection,
artifact workflows, separate generated-runtime consumers, OTP differential
execution, specialization measurements, cross-object inspection and
fault/lifetime/resource coverage. This does not automatically retire synthetic
tests: the case-level ledger must identify equivalent behavioral coverage first.

## Validation and open work

Historical evidence remains tied to its original host, reference and test
inventory:

- Preprocessing completed on 2026-09-18 with focused native/live-OTP validation;
  Linux/Windows were unvalidated at that stage. Per-step evidence remains in its
  contract, including copied headers and explicit upstream-crash handling.
- Parser validation on macOS arm64, 2026-09-19: C++23 Debug 46/46 and full
  quality; historical C++26 Debug 46/46, sanitizer and compiler/runtime/full
  configurations as recorded in the parser matrix. Phase V had 37 Debug and four
  focused ASan tests. Toolchain: CMake 4.4.2, Apple Clang 21, Boost 1.92, live
  OTP 29.0.5; historical source baseline was
  `751f87b703fe5948607d08e82599ce644b772e76`. C++26 is historical evidence, not
  the current supported language baseline.
- Projects on macOS arm64, 2026-09-20: each step passed a fresh compiler+runtime
  Debug build and full Lizard/clang-tidy. Per-step CTest totals for steps 1–22:
  `46,47,48,49,50,51,52,53,54,55,56,57,58,58,59,60,61,62,62,63,64,64`. Homebrew
  dependency discovery follow-up also passed 58/58 and explicit/absent-root
  reconfiguration. Step 21 passed Debug/compiler-only/ASan+UBSan 64/64 and
  runtime-only with absent TOML; step 22 checked documented examples, macOS
  wrapper, help/template, TOML blocks and links. Native path/alias capabilities
  are recorded in the matrix.
- Windows x64 migration, 2026-09-28, OTP 29.1.1: baseline 93 tests, 78 passed/15
  failed; migration inventory 75, initial Debug/Release 74/75 with
  parser-hardening failure; runtime ASan 15/15. These initial results did not
  establish a clean commit gate. Subsequent stack/frontend fixes closed
  Debug/quality failures; see the compiler history and migration ledger rather
  than rewriting the earlier result as passing.
- Historical compiler steps 24–27: fresh Windows x64 compiler+runtime Debug
  80/80 each, zero skips, full Lizard/clang-tidy and formatting/whitespace
  checks. Commits: `c86f539`, `f09e7b4`, `5db13f0`, `4301401`, respectively.
  This adds Windows evidence without completing the Linux x86/ARM or every
  32-bit/native platform obligation.

Later compiler evidence and current unfinished work follow. The original
migration failures above are historical; they were repaired before the passing
compiler gates.

## Compiler and runtime milestone

**Steps 1–46 complete, 2026-09-29.** Contracts and usage:
[compilation](../docs/compile.md), [semantics](../docs/semantic.md),
[specialization](../docs/specialization.md),
[feature reporting](../docs/features.md). Current validation:
[compiler matrix](../docs/validation.md). The numbered plan is
consolidated here; earlier stopping instructions are obsolete.

| Steps | Delivered behavior |
| --- | --- |
| 1–8 | LLVM SDK discovery/linkage, owned compilation/results, target policy, fresh verification, object emission, ABI v1 and feature reporting. |
| 9–14 | Runtime/context lifecycle, immediate terms, frozen generic builtin registries, lazy memory ownership, scheduler bookkeeping and explicit unavailable-service failures. |
| 15–23 | Declarations/exports, exhaustive subset checks, bindings and acyclic batch calls, symbolic declared types, independent inference and conservative contract warnings. |
| 24–28 | Target-width integers, parameter projections, local/remote calls, versioned descriptors and transactional module registration with native execution. |
| 29–32 | Bounded specialization planning/guarded variants, standard LLVM O0/O2, verified text/bitcode serialization. |
| 33–39 | Module artifact publication, command options, positional/project integration, verbose tracing, IR and declared/inferred type inspection. |
| 40–46 | CLI-generated native harnesses, OTP comparisons, specialization costs/caps, cross-target inspection, placeholder audit, resource/failure hardening and examples. |

### Delivered language and driver boundary

Accepted source has named modules/exports and single-clause functions with
distinct variable or wildcard parameters. One body expression may be a
tagged-small-integer literal (including negatives), parameter reference, or
nested direct local/literal remote call within an acyclic batch. Remote calls
require exports. Unsupported syntax is rejected even in unused functions;
parsing remains broader than compilation. Metadata is allowlisted;
behavior-changing attributes fail.

Owned side tables preserve the immutable AST. Declared type/opaque/nominal
aliases, remote visibility, specs/callbacks and constraints are modeled
symbolically and bounded. Independent inference propagates integer singletons
and argument/result relations; unknown inputs stay top. Specs warn on provable
contradictions but never narrow executable representation or supply guards.
Recursive types do not enable recursive functions. This is not full Dialyzer
analysis.

`-O0` stays generic; `-O2` enables useful bounded variants and LLVM O2.
`--no-type-specialization` wins regardless of option order. Limits are 3
variants per function, 32 per module, 128 per target and 2x pre-LLVM growth per
function/module. Generic ABI bodies/fallbacks remain. Current source has no
removable checks and correctly stays generic; synthetic guarded fixtures prove
hits/misses, rollback and caps without implying source guard support.

Positional inputs form one batch; selected project targets have independent
batches. Default compilation verifies object buffers in memory.
`--emit obj|llvm-ir|llvm-bc` publishes per-module artifacts; `--artifact-dir`
overrides roots and `--target-triple` selects the backend target. Encoded UTF-8
identities avoid filename collisions; target format determines suffixes. All
selected project targets validate before publication. Compilation failure
preserves outputs; multi-file replacement is not atomic if publication itself
fails partway through.

`--print-ir` / `--print-optimized-ir` produce verified snapshots without files
or machine code; combined snapshots retain stable module/stage order.
`--print-types` stops before LLVM. Action conflicts and frontend-only boundaries
are checked; `--verbose` traces begun phases and specialization decisions as
escaped `[comp]` events on stderr. Reached unsupported features report once and
fail explicitly. Successful lifecycle and sound generic fallback remain silent.

### ABI and runtime boundary

LLVM owns optimization and machine/object generation; Clang links native
consumers. Verification is not proof of Erlang semantics. Compiler LLVM
dependencies stay private; runtime-only builds need Boost Multiprecision, not
LLVM, Boost.Parser, TOML or OTP. SDK/toolchain provenance remains in the
compilation contract and ledger.

ABI v1 uses unsigned target words, checked signed 28/60-bit integer payloads and
collision-free module/function/arity symbols. Generic entries take a live
context and aligned term array (null at arity zero), returning a term word.
Width/alignment come from target layout. APIs are internal C++23; historical C
wrappers were removed. GC, exceptions and suspension may require ABI revision.

Every runnable generated program links one matching runtime through
`ErlangAoT::generated_program`. Descriptors validate ABI/word width;
transactional registration copies names. Runtime owns stable contexts,
CodeServer and SchedulerService; contexts own lazy heap/mailbox and invalidate
lifetime tokens before teardown. Scheduler dispatch records only metadata. Host
Terms admit small integers and exact empty tuple/list; structural atom/pid/port
tag recognition does not validate identity. Heap pointers are not dereferenced.
Immediate copies are owner-independent; they are not future graph-copy
operations. Frozen registries use exact name/arity/all-Term keys; resolved calls
pin module/code lifetime. Publication/lookup remain host-serialized. Service
bridges separate status from output and contain exceptions without fabricating
success.

Runtime contracts: [lifecycle](../docs/runtime.md#lifecycle),
[terms](../docs/terms.md), [builtins](../docs/runtime.md#code-server-and-builtins),
[memory](../docs/runtime.md#process-memory), [scheduler](../docs/runtime.md#scheduler-bookkeeping)
and [services](../docs/runtime.md#deferred-services). Proposals in `runtime/include/` and
`runtime/include/unverified/` are not completed APIs merely because headers
exist. Carry forward ownership contracts from
[terms](../runtime/design/terms.md),
[processes](../runtime/design/processes.md),
[atoms](../runtime/design/atom_storage.md) and
[code server](../runtime/design/code_server.md) when filling the gaps.

## Compiler validation history

Each numbered step recorded a fresh compiler+runtime Debug build, full CTest and
Lizard/clang-tidy gate before its individual commit, with formatting/whitespace
checks. Historical counts/revisions below are evidence, not current suite
targets. Cross-object inspection never establishes native execution. Current
inventory, commands and limits:
[validation](../docs/validation.md).

### SDK and ABI: steps 1–8 (2026-09-24, macOS arm64)

Global Homebrew LLVM 23.1.1_1 / SDK 23.1.1 was used. Native Linux/Windows
execution was pending at these checkpoints; focused ASan/UBSan passed where
listed, but LeakSanitizer was unavailable.

| Step | Delivered and validated | Full CTests |
| --- | --- | --- |
| 1 | Frozen subset, artifact/CLI and provisional ABI contract; SDK paths, versions and documentation links. | 65 |
| 2 | Private codegen target and global-only SDK discovery; selection/rejection policies and LLVM-free runtime-only build. Only one distinct global installation was available. | 67 |
| 3 | Move-only compilation ownership, AST provenance, isolated LLVM contexts, durable results and diagnostic-failure containment; opaque consumer and focused ownership/result ASan/UBSan. | 69 |
| 4 | Native/foreign target policy, PIC/Small layouts, backend initialization and failure handling; 32/64-bit layouts, moves/reuse and static-component LLVM linkage. | 70 |
| 5 | Fresh function/module verification, target consistency and batch invalidation; malformed IR, post-verification mutations and result lifetimes. | 71 |
| 6 | Reverified, repeatable object emission from cloned modules; Mach-O arm64 symbol/section inspection, foreign ELF/COFF, assembler failures and static LLVM linkage. | 72 |
| 7 | ABI v1, checked 32/64-bit integer codecs and private layout assertions; boundary/tag tests, cross-target signatures/objects, six-triple C header checks and integer ASan/UBSan. Runtime-only: 3 tests. | 75 |
| 8 | Shared 23-feature catalog, escaped context and separate compiler/runtime reporting; once-only diagnostics, sink failures and artifact cleanup. Runtime-only: 6 tests; reporting ASan/UBSan and six-triple C status checks. | 80 |

### Runtime skeleton: steps 9–14 (2026-09-25, macOS arm64)

Native Linux/Windows/32-bit runtime execution and LeakSanitizer remained
pending. Runtime-only builds stayed LLVM-free; steps 10–14 also recorded Release
validation. Allocation-failure injection covered rollback and cleanup; no heap
allocation, workers or generated Erlang execution were claimed.

| Step | Delivered and validated | Full CTests | Runtime-only / focused ASan/UBSan |
| --- | --- | --- | --- |
| 9 | Runtime/context lifecycle, stable identities, token invalidation and ordered teardown; independent lifetimes, busy preservation, rollback and mandatory generated-program runtime linkage. | 84 | 10 tests; lifecycle/failure checks |
| 10 | Structural immediate-term classification and checked native integer services; malformed/pointer-shaped values, private layouts and agreement with LLVM constants. | 86 | 11 tests; 3 term tests |
| 11 | Frozen module registries, pinned generic dispatch, immediate-only Terms and status/output bridge; publication rollback and missing/unavailable BIF reporting. | 89 | 14 tests; 4 dispatch/failure tests |
| 12 | Lazy process memory ownership, checked budgets and allocation-free immediate copying; explicit unavailable allocation/collection and future root/resource contracts. | 90 | 15 tests; 5 memory/lifecycle tests |
| 13 | Scheduler registration and lifecycle bookkeeping; checked transitions, teardown order and registration rollback/retry. | 91 | 16 tests; 5 scheduler/lifecycle/memory tests |
| 14 | TermFactory, memory, send, execution and unload reporting boundaries; known-deferred versus unknown BIFs, state preservation and once-only propagation. | 93 | 18 tests; 6 service/memory/dispatch/failure tests |

After step 9, a user-directed C++ API revision removed the C headers and
lifecycle adapter. Namespaced ABI constants, scoped status and the real
ProcessContext became the sole project-internal interface, retaining the native
generated-function machine convention. The full 84-test gate, runtime-only 10
tests, focused sanitizers, C++ consumer and archive-symbol checks passed again.
Earlier C compilation/link/run and six-triple header evidence refers to the
superseded API, not the current contract. Minor destructor/test expectation
findings in steps 11–14 were corrected before the final passing gates; no checks
or thresholds were weakened.

### Windows transition and reference refresh (2026-09-28)

The initial steps 14–19 attempt reproduced existing exception-escape/Boost
analyzer findings and the raised-depth parser crash; it advanced no numbered
step. The OTP reference moved to official `maint-29` at
`21776803ecd11f5fa948732c0ec66b8f325dedfc`; the grammar audit and ten-file
corpus retained their hashes/witnesses. Subsequent upstream checks through step
27 found that revision unchanged. Historical validation above retains its
original context.

A prerequisite repair restored all 75 CTests and the full quality gate: Windows
executables reserve 8 MiB stacks, movable frontend storage avoids throwing
moves, and numeric/binary-slice/CLI boundaries were clarified. Real CLI tests
include largest-finite-binary64 conversion. One analyzer crash inside
Boost.Parser passed on an unchanged complete rerun
(`build/compile-steps/quality.log`).

### Semantic analysis, lowering and specialization: steps 15–30 (2026-09-28–29, Windows x64)

These gates used Clang/SDK 23.1.2 from the pre-existing `thirdparty/`
installation (no SDK download) and pinned clang-tidy 22.1.8 selected via
`CMAKE_PROGRAM_PATH`. Nondiagnostic launcher/analyzer failures in early runs
passed on complete reruns; from step 18, Windows analysis defaults to two
concurrent jobs to bound memory. All production commands, checks and thresholds
remained enabled.

| Step | Delivered and validated | Full CTests |
| --- | --- | --- |
| 15 | Module/function/export indexing, located CLI errors and reversible ABI symbols; real positional/project cases and 147 symbol round trips. | 76 |
| 16 | Exhaustive subset checks, including unused bodies/nested arguments, metadata policy and explicit negative integer bounds. | 76 |
| 17 | Catalog-owned capability diagnostics and defensive lowering boundaries; distinct compiler send/sequence IDs and failed-output invalidation. | 76 |
| 18 | Parameter-position side tables; wildcard slots, repeated/unbound names and include provenance. | 76 |
| 19 | Owned per-target batches, exact local/remote resolution and iterative cycle rejection; nested/forward calls, 501-function chains and target isolation. | 76 |
| 20 | Owner-checked symbolic type graph, exhaustive AST categories, canonical unions and bounded widening; temporary lattice/ownership invariants. | 77 |
| 21 | Declared aliases, visibility, opaque/nominal identity, specs and constraints; finite recursion, bounded substitution and OTP-checked scope rules. Unknown external metadata warns; invalid batch declarations fail. | 78 |
| 22 | Independent bounded implementation inference; integer singletons and parameter relations unaffected by annotations, with unknown inputs and safe widening. Calls remained unknown at this step. | 79 |
| 23 | Fresh call-summary instantiation and conservative contract warnings; escaped opt-in `--impldebug` output, nested remote calls and target isolation. Final full quality covered 151 production commands. | 79 |
| 24 | Generic declarations and checked target-width tagged literals; verified native objects, 32/64-bit endpoints and narrower-target overflow rejection. | 80 |
| 25 | Aligned parameter-array loads preserving terms and source positions; grouped identity, wildcard projections and native/32-bit objects. | 80 |
| 26 | Resolved local calls with iterative source-order argument evaluation, aligned arrays and context forwarding; forward/private/nested calls and identical arguments. | 80 |
| 27 | Separate-module remote declarations and matching object imports/definitions; `answer`/`client` in both input orders, plus private/missing/cycle/duplicate-module diagnostics. | 80 |
| 28 | Versioned module/export descriptors, retained explicit startup and transactional frozen runtime registration; linked real-source execution, rejection/lifetime tests and missing-runtime link failure. | 82 |
| 29 | Canonical implementation profiles, exact-check benefit recognition, deterministic count/work/growth limits and generic no-benefit fallback. | 83 |
| 30 | Guarded LLVM variants, unchanged public ABI and retained generic bodies; measured 2x growth rollback, native hit/miss equivalence and exhausted-inference fallback. | 84 |

Steps 24–27 each passed 80/80 with zero skips (commits `c86f539`, `f09e7b4`,
`5db13f0`, `4301401`); step 27 quality covered 154 production commands. Steps
28–30 passed 82/83/84 tests; steps 29/30 covered 159/162 production commands.
Unchanged analyzer crashes passed complete retries without
exclusions/suppressions. Step 23's debug-silence assertion was corrected before
its passing rerun.

### Public workflows and closure: steps 31–46 (2026-09-29, Windows x64)

Same Clang/SDK 23.1.2 and clang-tidy 22.1.8; official `maint-29` remained
`21776803ecd11f5fa948732c0ec66b8f325dedfc`. All rows passed full CTest and
quality. Native generated-program execution here is Windows x64 evidence only.

| Step | Validated behavior | Full CTests | Production quality commands, where recorded |
| --- | --- | ---: | ---: |
| 31 | O0/O2 consumers preserve registration, results and ABI. | 85 | — |
| 32 | SDK text/bitcode round trips and invalid snapshots; corrected unterminated test assembly. | 86 | 164 |
| 33 | Unicode/encoded paths, replacement, collisions, input aliases and failure cleanup. | 87 | 167 |
| 34 | Both CLI modes: option operands, repetition, conflicts and informational no-I/O precedence. | 88 | 168 |
| 35 | Positional artifacts, failed batches and frontend compatibility; LLVM remark filters respected. | 89 | 170 |
| 36 | Project order/isolation/roots and no publication on later-target failure; unchecked optional backend policy removed. | 90 | 172 |
| 37 | Phase ordering, escaping, early failures, specialization reasons and unchanged artifact bytes. | 91 | 176 |
| 38 | Before/after IR round trips, projects, conflicts and no writes; SDK FileCheck unavailable. | 92 | 177 |
| 39 | Types, aliases, callbacks, warnings and unknowns; renamed test script shadowing Python types. | 93 | 180 |
| 40 | CLI objects link/run with the real runtime at O0/O2; immediate boundaries, calls, ABI rejection, missing-runtime failure and teardown. | 95 | — |
| 41 | 150 seeded/fixed calls match OTP and an independent evaluator under four policies, each twice; wrong/missing specs preserve behavior. | 96 | — |
| 42 | Source/synthetic measurements; 255-argument/64-member-union inputs, byte-identical source O2 policies, count/growth caps and native fallback. | 97 | — |
| 43 | O0/O2 ELF/Mach-O/COFF objects for seven targets: symbols/imports, widths/tags, integer endpoints and failure without publication. | 98 | — |
| 44 | Both CLI modes audit compiler placeholders; executable output fails explicitly; rejected runtime allocation preserves state/calls. | 99 | — |
| 45 | Batch/AST/writer ceilings, partial/close/interrupted writes and Debug STL OOM repairs retaining iterator checks. | 102 | 182 |
| 46 | Two-module example at O0/O2; artifacts, inspections, specialization override; documented SDK/ABI/runtime recipe and inventory. | 103 | 182 |

Steps 40–46 report zero skips. Step 45 also passed compiler-only Debug 80/80,
runtime-only Debug 16/16 and runtime-only ASan Release 16/16. Runtime ASan uses
`/MT` and matching thunks; it instruments neither the SDK nor emitted Erlang.
Full compiler ASan was unavailable due to the SDK's MSVC STL annotation ABI
(`annotate_string` 1 versus 0); earlier attempts hit rpmalloc/CRT duplicate
symbols. Neither limitation was suppressed or counted as passing coverage.
Logs/scripts remain under ignored `build/compile-steps/`; the tracked
103-test inventory (Git history) and matrix preserve reproducible
scope.

<a id="completed-patternmatch"></a>

## Pattern matching and guards: steps 1–20 (2026-10-01–03, Windows x64)

[The retained completed-plan context](11-plan.md#completed-patternmatch)
includes added step 15a and the necessary F02/F03/F06/F08–F17/F20/F26 slices.
Execution is scoped to ordered function clauses, their guards, body
matches/sequences and acyclic local/exported remote calls. Admitted values are
owned atoms, arbitrary integers, finite floats, tuples, proper/improper lists,
strings, maps, bitstrings and ordinary tuple records.
[The semantic matrix](../docs/patterns.md) and linked contracts
define current behavior; older milestones above retain their original revisions
and scope.

Every implementation step passed its fresh combined Debug build, CTest, Lizard
and clang-tidy gate before its separate commit. The table preserves each
checkpoint's test count; the decrease at step 15a reflects moving live OTP
audits behind an explicit opt-in, rather than dropping golden native coverage.

| Step and validation | Delivered and validated | Full CTests | Production quality units, where recorded |
| --- | --- | ---: | ---: |
| [1](../docs/validation.md#history) | Pinned semantic matrix and 81 exact source signatures; acceptance/oracle evidence, suite syntax and unchanged native helper baseline. | 104 | — |
| [2](../docs/validation.md#history) | Revision-2 checked generated-call failure channel, owned first failure, nested/reentrant cleanup, ABI rejection and retry. | 108 | — |
| [3](../docs/validation.md#history) | Bounded runtime-owned atoms/booleans, spelling pins, foreign-word rejection and revision-3 transactional module bindings. | 109 | 189 |
| [4](../docs/validation.md#history) | Clause-local definitions/reads/exact checks, tentative scopes, RHS-first binding, located unsafe reads and bounded conservative facts. | 111 | 191 |
| [5](../docs/validation.md#history) | Flat bounded pattern normalization, constant arithmetic, aliases, incoming map keys and preceding-segment binary-size scopes. | 113 | 196 |
| [6](../docs/validation.md#history) | Immediate literal/repeated/alias/wildcard matching and checked shared equality; mismatch, nested failures, 34 OTP/native calls and both-width objects/IR. | 114 | 199 |
| [7](../docs/validation.md#history) | Guard resolution, immediate predicates/comparisons/queries, legacy and qualified calls, semantic/infrastructure failure separation; 1,689 outcomes. | 119 | 205 |
| [8](../docs/validation.md#history) | Comma/semicolon alternatives, canonical true, strict/lazy boolean control flow and owned body badarg payloads; 2,075 outcomes. | 120 | 207 |
| [9](../docs/validation.md#history) | Ordered candidate isolation/fallback, whole-function analysis, conservative joins and function_clause exhaustion; 1,020 outcomes. | 121 | 208 |
| [10](../docs/validation.md#history) | Single-evaluation RHS-first body matches/chains, success-only bindings, sequences and owned badmatch/retry; 1,666 outcomes. | 122 | 209 |
| [11](../docs/validation.md#history) | Bounded stable heaps/reservations, explicit resource destruction and revision-4 generated roots/result handoff; allocation/root faults and old-ABI rejection. | 123 | 213 |
| [12](../docs/validation.md#history) | Rooted tuple/list/string construction, structural comparison and matching; retained compound results/errors, fault cleanup and 4,801 outcomes. | 125 | 221 |
| [13](../docs/validation.md#history) | Owned arbitrary integers, exact arithmetic/bitwise operations, promotion/demotion, checked fast paths and integer guards; 16,065 outcomes. | 127 | 231 |
| [14](../docs/validation.md#history) | Finite binary64 arithmetic/conversions, exact mixed comparisons, signed zero and rooted failures; 14,436 outcomes. | 129 | 238 |
| [15](../docs/validation.md#history) | Immutable exact-key maps, source-ordered construction/updates, computed-key matching and owned map errors; 8,010 outcomes. | 131 | 244 |
| [15a](../docs/validation.md#history) | Project-owned source/call/result goldens and provenance manifests; OTP-free normal builds/tests, explicit regeneration and opt-in live audits. | 118 | 244 |
| [16](../docs/validation.md#history) | Small/shared bitstrings, checked numeric/UTF segments and cursors, retained tails, queries/parts and bit-accurate matching/comparison; 8,826 outcomes. | 120 | 253 |
| [17](../docs/validation.md#history) | Ordinary record declarations/defaults, tuple construction/access/matching and record tests; owned badrecord, declaration order and 1,025 outcomes. | 121 | 257 |
| [18](../docs/validation.md#history) | Admitted-domain guard catalog, inclusive is_integer/3 and legacy resolution audit; 77 implemented rows, four dependency gates and 5,033 outcomes. | 122 | 257 |
| [19](../docs/validation.md#history) | Budgeted whole-value binding facts, conservative extraction/joins, wrong-spec equivalence, verified generic fallback and 976 CFG proof observations; 822 paired outcomes. | 123 | 258 |
| [20](../docs/validation.md#history) | Provenance/coverage reconciliation, seeded deep/wide stress, all eight driver/policy combinations, fault/limit/publication recovery and updated runnable example. | 124 | 258 |

Current generated descriptors use **ABI revision 4**; the revision-2 checked
failure channel remains unchanged. Candidate bindings publish only on
match/guard success; body matches evaluate their RHS once. Every fallible
service checks before output use, and representation/shape proofs dominate
extraction. Specs do not authorize runtime access. Reached semantic guard errors
reject the enclosing alternative; resource/ownership/internal failures stop
execution. Returned terms and offending error values retain ownership before
root cleanup; expired context handles deny access. Stable backing does not
implement GC or graph copying.

Official maint-29 was re-fetched through 2026-10-03; pin, clean checkout and
upstream remained `21776803ecd11f5fa948732c0ec66b8f325dedfc`. Oracle OTP 29.1.1
/ ERTS 17.1; host/SDK LLVM 23.1.2, Lizard 1.24.0 and clang-tidy 22.1.8. Explicit
regeneration reproduced all **19 owned corpora**: **67,634 native expected
values/error reasons**, plus **106 separate semantic acceptance rows**. Normal
tests require neither OTP nor its checkout and never refresh goldens or the
reference silently. Manifests retain licenses, hashes, revisions and exact
helper/adaptation provenance; suite parsing is syntax evidence, not Common Test
execution.

The final native corpus runs both positional/project drivers at O0/O2 with
specialization enabled/disabled, local/remote calls and two executions per
combination: **1,082,144 golden comparisons**. Seed `0x29A07` adds 1,969
outcomes, depth 64, width 255 and 128 ordered alternatives. The documented
two-module demo prints `42`, `-7`, `record`, `map`, `binary`, `list`, `integer`,
`other` and passes all four policies. Fault seams and inaccessible
work/IR/allocation ceilings remain separate from source execution evidence.

Final fresh Windows x64 Debug: **124/124 CTests**, zero skips, **167.43 s**, and
all **258 production quality units** pass at unchanged CCN/cognitive-complexity
thresholds of 10. [Final validation](../docs/validation.md#history)
and [evidence](../docs/validation.md#history) retain concrete
corpus/ catalog/test identities and log hashes. The initial closure run exposed
the test transport's depth-256 limit around a returned 255-cell list; its
bounded test-only allowance became 512 before the passing fresh run. Production
and quality limits were unchanged; initial logs and all historical validation
records are preserved.

Four catalog signatures remain explicitly unavailable: `self/0`, `node/0,1` and
native `is_record/1`. Positive pid/port/reference/function values, record
updates/ record_info/native forms, other control/guard contexts, handlers,
recursion and process execution retain their backlog owners. Native Linux, Apple
Silicon and 32-bit execution and new compiler/frontend sanitizer runs remain
unavailable; foreign objects, 32/64-bit IR/layout checks and historical
macOS/runtime-ASan evidence retain their separate scope. Completing this plan
closes only its delivered function/body/guard and prerequisite
representation/service slices.

## Outstanding work to finish

The [feature backlog](01-todo.md) expands these gaps into explanations and
implementation checklists for separately chosen detailed plans.

**These are unfinished features or validation obligations, not completed
compiler steps.** The 46-step immediate-term milestone and scoped pattern/guard
plan (steps 1–20 plus 15a) are complete; the overall Erlang-to-native goal still
requires the following work.

- [ ] **Production executables:** startup/entrypoint policy and native linking
  with the matching runtime; replace explicit executable-output failure.
- [x] **Scoped executable matching and guards:** ordered function clauses, body
  matches/sequences, exact bindings, grouped/strict/lazy guards and the admitted
  checked service catalog; see
  [the completed-plan context](11-plan.md#completed-patternmatch).
- [x] **Admitted terms and stable allocation:** rooted arbitrary integers,
  finite floats, tuples/lists/strings, maps, bitstrings and ordinary tuple
  records; checked construction/access/comparison/arithmetic, owned
  results/errors and fault cleanup.
- [ ] **Additional Erlang semantics:** case/if/maybe/comprehension and
  receive/fun/ catch guard contexts, record updates/record_info/native forms,
  closures/dynamic calls, source exception handling and recursion/proper
  bounded-stack tail calls.
- [ ] **GC and graph copying:** rooted cross-process transfer and collection,
  relocation/safepoints and continuation/mailbox/cursor/in-transit roots with
  their concrete owners; extend validated layouts for future representations.
- [x] **Atoms:** stable bounded storage, generated spelling/slot bindings,
  atom/boolean literals and retained host/error ownership; see
  [step 3](../docs/validation.md#history).
- [ ] **Identities and atom GC:** owned pid/port/reference services, atom
  collection and synchronized access before worker integration remain deferred.
- [ ] **Processes and scheduling:** cooperative generated execution, reductions,
  workers/queues, wakeups, signals, send and selective receive. Preserve
  per-sender order; all messages, including self-send, enter the signal inbox
  before owner-side copying/mailbox insertion. Compare continuations with LLVM
  coroutines before suspension; current scheduler records do not run processes.
- [ ] **Runtime services:** generic production BIF registration and additional
  families, typed/native callable integration/conversions, concurrent module
  publication/lookup and cooperative generated calls. Compiler-authorized
  admitted guard services are delivered; they do not close the generic
  production bridge. Dynamic loading/unload/code upgrades remain deferred and
  may be omitted under the project scope; resolve that choice explicitly before
  promising support.
- [ ] **Later code generation tooling:** useful source-driven specialization as
  the subset expands, debug information, profiling and LTO.
- [ ] **Native platform validation:** current compiler/runtime/harness execution
  on Linux x86/x64/ARM/AArch64, macOS Apple Silicon and Windows x86. Historical
  macOS skeleton evidence and seven inspected object targets do not close this
  matrix.
- [ ] **Sanitizers and compatibility:** full compiler/frontend ASan, UBSan and
  LeakSanitizer with a compatible SDK; retain lifetime/failure tests. Full
  upstream OTP Common Test suites remain unrun. Refresh `maint-29` per the
  reference procedure at the next OTP-dependent task while preserving historical
  revisions.
- [ ] **Test migration closure:** retire adapters/synthetic successes only after
  case-level equivalent public coverage; retain justified API-only invariants,
  injected failures and cross-width boundaries.

Explicitly deferred rather than required for this milestone: public interchange,
C/FFI compatibility, intermediate-stage readers (reserved directories only), and
project-schema extensions such as dependencies, imports, profiles, package
fetching, watch/cache or parallel execution. Do not prebuild these as part of
closure. Future code changes retain AGENTS.md's formatting, meaningful
behavioral tests and fresh combined Debug `check-quality` gate with no weakened
thresholds/suppressions.
