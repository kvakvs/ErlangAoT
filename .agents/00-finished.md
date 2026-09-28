# Completed foundations and frontend work

Compact implementation record, consolidated 2026-09-28. This archive preserves
completed work, compatibility decisions, historical evidence and open obligations.
Current behavior lives in the linked documentation; current architecture and file
ownership are in [arch.md](arch.md) and [files.md](files.md).

| Area | Delivered status | Remaining qualification |
| --- | --- | --- |
| Foundations | CLI/CMake scaffold, 2026-09-17 | Production executable pipeline remains active compiler work. |
| Preprocessor | Steps 1–13 implemented, 2026-09-18 | Focused OTP comparisons; no full upstream Common Test run. |
| Parser | Steps 1–17 complete; step 18 implementation and macOS validation, 2026-09-19 | Required platform matrix is not fully closed. |
| Projects | Steps 1–22 complete, 2026-09-20 | Historical host evidence and later Windows evidence remain distinct. |
| Test migration | Available frontend/project/runtime migrations implemented, 2026-09-28 | Generated-program migration and frontend sanitizer closure remain deferred. |

[04-compile.md](04-compile.md) owns ongoing compiler/runtime work: steps 1–27 are
complete, 28–46 pending. The user requested stopping before step 28. Steps 24–27
validate lowering through real-source stage adapters; they do not publish CLI
artifacts or execute generated Erlang programs.

## Foundations

The project targets Erlang/OTP 29 on Windows x86-family, Linux x86/ARM and macOS
Apple Silicon. The delivered build uses C++23, CMake 3.28+, warnings as errors,
Clang as the reference toolchain, and separate compiler/runtime enablement.
`erlang_aot` produces `erlangaot`; `erlang_runtime` is independently buildable;
`erlang_aot_abi` carries shared build contracts. The runtime does not depend on
compiler internals or LLVM. Cross builds separate the host compiler from the
target runtime, with matching generated-code/runtime ABI and toolchain settings.

`compiler/` owns driver, source/diagnostics, lexing, preprocessing, parsing, AST,
semantics and LLVM code generation. `runtime/` owns terms, allocation, process
contexts, scheduling and module dispatch; `abi/` owns shared internal contracts.
`cmake/`, `tests/`, `examples/` and `docs/` hold build rules, verification, workflows
and documentation. See the current file map for exact implementation locations.

Stage boundaries use owned internal data. Only directory locations are reserved
for `compiler/src/stage_readers/{preprocessed,abstract,ir}/`; readers and a public
interchange format are not implemented. APIs remain project-internal C++23;
C linkage and external interoperability await a concrete need. Earlier selectable
C++26 and C-wrapper proposals are superseded. Diagnostics retain source/macro/
include context, use stderr and report failure through exit status.

## Preprocessor

Contract, limits and per-step evidence: [preprocessor.md](../docs/preprocessor.md).
The implementation is independent of LLVM/runtime and processes each source in
an isolated session. Boost.Parser remains a pinned character-parser dependency;
the rejected token-iterator experiment is not a basis for another token adapter.
Expanded tokens are handled explicitly and retain owned source spellings.

| Steps | Delivered behavior |
| --- | --- |
| 1–3 | Parser/oracle boundary; source buffers, encoding and locations; Erlang lexer; form/directive dispatch, recovery and diagnostic events. |
| 4–6 | Object and parameterized macros, overloads/zero arity, source-order definitions/undefinition, recursion checks, substitution/rescanning and original-token stringification. |
| 7–9 | Nested conditionals and skipped forms; include/include_lib lookup; contextual module/file/line/function/machine/OTP macros. |
| 10–11 | Restricted preprocessing expression evaluator; feature configuration, keywords and feature-query macros. |
| 12–13 | Warning/error directives, failure latching, provenance, CLI options, live OTP/golden fixtures and copied-header smoke coverage. |

Lexing supports arbitrary decimal integers, decoded Unicode, sigils and triple
strings. Macro argument scanning balances delimiters and distinguishes fun types/
references from blocks. Object and parameterized definitions may coexist;
stringification uses the original argument tokens. Logical `-file` context stays
separate from physical spelling. Includes preserve nested sibling precedence,
explicit application roots and documented environment handling.

Condition evaluation supports the allowed guard-expression subset, exact integers,
terms, bitstrings and exact map keys. Only `true` selects a branch; ordinary
arithmetic/evaluation failure selects false, while malformed syntax and resource
failures diagnose. It never executes arbitrary Erlang source. Conditional structure
is still checked inside skipped regions. Contextual function macros recognize
headers without embedding a full parser in preprocessing.

Production feature metadata distinguishes approved/default-enabled `maybe_expr`
from experimental/default-disabled `compr_assign`; OTP synthetic test features
are excluded. Expansion, include nesting, token production, expression depth and
integer/bitstring size are bounded; defaults and diagnostic policies are in the
contract. Full parsing, binding, record expansion, transforms and external
documentation ingestion belong to later stages or remain outside this scope.

## OTP preprocessor test reference

The initial inspection on 2026-09-18 used OTP-29.1 commit
`751f87b703fe5948607d08e82599ce644b772e76` (the annotated tag object is different).
It inspected source and selected fixtures; the checkout was not built and the full
upstream suites were not executed. Current research tracks `maint-29`, recorded
as `21776803ecd11f5fa948732c0ec66b8f325dedfc`. Follow
[otp-reference.md](../docs/otp-reference.md) before future OTP-dependent work:
refresh upstream, checkout/pin, corpus hashes, grammar evidence and current docs
together; retain historical revisions and never refresh silently during tests.
`references/otp` is ignored research material, not a submodule or normal build input.

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
  `features_include`, `features_disable`, `features_all`, `features_erlc_unknown`,
  `features_atom_warnings`; data `f_macros.erl`, `f_directives.erl`, `f_disable.erl`,
  `f_include_1.erl`–`f_include_3.erl`, `f_include_exp2.erl`.
  `OTP_TEST_FEATURES=true` enables synthetic features, not product defaults.
- `lib/compiler/test/compile_SUITE.erl`: `cond_and_ifdef` with `simple.erl`,
  `makedep`, `listings`, `deterministic_include`. Dependency/listing formats are
  separate compiler behavior, not preprocessing token compatibility.
- `lib/stdlib/test/erl_scan_SUITE.erl`: `sigil_string`, `triple_quoted_string`,
  `otp_7810`; implementations `erl_scan.erl` and `erl_features.erl`.

Preserve OTP details: non-boolean `42` is false in conditions; fun types need no
`end`; duplicate macro parameters and nonparameter `??B` follow OTP behavior;
macro-generated function heads matter; feature-enabled `maybe`/`else` alter token
classification. Compare preprocessing with `epp:scan_erl_form`, parsing with
`erl_parse`, and compilation/execution separately. Normalize fixture roots and
binary64 float bits; implicit file attributes have separate native checks.

Copied `otp_assert.hrl`/`otp_file.hrl` fixtures retain upstream Apache-2.0 notices
and provenance. Keep upstream-crash exceptions explicit (including circular
object macros) and native diagnostics covered. Full upstream execution requires
a matching built OTP and its documented Common Test hooks; bare `ct_run` against
an unrelated installation does not establish compatibility.

## Parser

Contracts and historical evidence: [parser.md](../docs/parser.md) and
[parser-validation.md](../docs/parser-validation.md). The parser consumes existing
expanded ordinary-form tokens directly: no printing/relexing or second lexer.
Small cursor/delimiter/operator mechanisms are shared; the preprocessor's
restricted evaluator is not generalized into the full parser.

| Steps | Delivered behavior |
| --- | --- |
| 1–4 | Pinned grammar/oracle inventory, shared token mechanics, AST ownership/provenance and transactional form parsing. |
| 5–7 | Literals/aggregates, operators/calls/remotes, function clauses, pattern syntax and guards. |
| 8–9 | Maps, unresolved records and binary/bitstring syntax. |
| 10–12 | Blocks, branching, receive, funs, try/catch, maybe and strict/zipped comprehension forms. |
| 13–15 | Ordinary/documentation/record attributes, type/opaque/nominal declarations, specs/callbacks and constraints. |
| 16–18 | Bounded diagnostics/recovery, parse-check integration, structural differential coverage, pinned corpus and available host validation. |

Move-only module-owned arenas provide distinct checked expression, pattern-syntax,
type and form IDs, typed payloads, exact integers and explicit optional/nonempty
constructs. IDs survive arena growth; borrowed references do not. Source metadata
retains logical anchors, expanded extents and physical spelling/origin tables
through macro/include boundaries without inventing contiguous cross-file ranges.
Completed ASTs remain readable after preprocessing sessions/source managers die.

Parsing is transactional per form: failure rolls back ordinary nodes, diagnostics
mark the module failed and later valid forms can survive. Node count, depth, work,
diagnostics and printer traversal are bounded. Completed trees are immutable and
retain records/defaults/types/features without desugaring. Documentation-file paths
are data, not reads; parse-transform attributes do not execute transforms.

Syntax acceptance is separate from binding, guard legality, linting and type
checking. Restricted `pat_expr` applies to function/fun heads; other pattern
candidates can remain permissive. Guards preserve comma/semicolon grouping and
parse general expressions. `spec`, `callback` and `record` are contextual prefixes.
OTP parser-builder normalization belongs here; record expansion and `v3_core` do
not. Parsing a feature's syntax does not enable it.

CLI modes `--parse-check`, `--print-pp` and `--print-ast` are delivered; the current
printer contract is documented separately and is not a public interchange format.
Structural oracle projections fail on unmapped nodes, normalize only documented
OTP/grouping differences, and retain native provenance/ownership invariants.

Historical closure: 344 ordinary grammar productions witnessed, 79 SSA exclusions,
423 total inventory rows; 43 positive authored fixtures, 183 ordinary negatives,
three OTP builder exceptions (`record_helper`, `record_extra`, `any_first`) and
original `bad.erl` coverage. The ten-source pinned corpus covers `lists`, `maps`,
`sets`, `erl_scan`, `beam_ssa`, `beam_asm` and the four headers `beam_ssa.hrl`,
`beam_asm.hrl`, `beam_opcodes.hrl`, `beam_types.hrl`; hashes are in
`tests/fixtures/parser/phase6/otp.tsv`. This proves syntax coverage for that corpus,
not semantic acceptance or a full upstream Common Test run.

## Projects

All 22 steps delivered TOML projects, target selection and safe manifest creation.
Current usage/schema: [projects.md](../docs/projects.md); historical matrix:
[project-validation.md](../docs/project-validation.md); example:
[project.toml](../examples/project/project.toml). Private project code owns toml++
3.4.0 discovery, model/loading/schema, paths/globs/discovery, selection/options,
planning/execution, CLI and creation; driver hooks share the per-file frontend.
Runtime-only builds do not discover TOML dependencies.

| Steps | Delivered behavior |
| --- | --- |
| 1–6 | Format contract, private dependency, owned located model, bounded loading, strict declarations and typed frontend options. |
| 7–10 | Path bases/fallback, bounded Unicode globs/traversal, deterministic source assembly and physical deduplication. |
| 11–14 | Ordered selection, effective options, full selected-target preflight and reusable frontend request. |
| 15–19 | Target execution, project/target CLI, annotated template, exclusive creation and creation CLI. |
| 20–22 | Real workflows, resource/native-path portability coverage and published examples. |

Version 1 requires integer `schema_version = 1`, nonempty targets, unique
case-sensitive ASCII names matching `[A-Za-z0-9_][A-Za-z0-9_.-]*`, and a nonempty
`sources` or `source_dirs` selection. Unknown keys, wrong types and duplicates
are errors throughout the manifest, including unselected targets. Defines reuse
the preprocessor's Erlang literal parser (`NAME` means `true`); duplicates and
conflicting feature lists fail. Arbitrary shell/backend options are not accepted.

`--project` excludes positional sources; repeated `--target` selects unique targets
in first-request order, otherwise all run in manifest order. Unknown selectors
return usage status 2 and list available names. Help/version validate syntax but
do no filesystem work. Missing suffixless project operands can resolve `.toml`.
All selected targets are planned before frontend execution; filesystem validation
is limited to selected targets. Later file/target work continues after frontend
errors, with isolated settings, contextual diagnostics and latched failure.

Manifest paths use the lexical manifest parent's base, including symlinked
manifests; CLI paths use invocation cwd. External/`..` paths are permitted, with
no shell/tilde expansion. Literal missing sources try ordered search roots;
existing invalid candidates fail. Search roots neither discover sources nor find
headers. Globs support case-sensitive `*`, Unicode-scalar `?` and whole-component
`**`; bracket/brace/negation/escape syntax is rejected. Hidden entries participate;
there are no implicit build/vendor exclusions. Directory discovery skips symlinks,
explicit directory roots may be symlinks, and selected dangling file links fail.

Preserve source-entry order, sort each expansion by generic UTF-8 bytes, append
`source_dirs`, and deduplicate physical identity within each target, retaining the
first spelling. CLI include paths precede manifest includes (last CLI `-I` first);
CLI applications replace matching mappings and feature settings apply last.
Macro duplicates fail instead of replacing definitions.

Outputs reserve future executable paths, defaulting to `build/<target>[.exe]`.
CLI `-o` requires one compilation target and is forbidden for check/print modes;
those modes ignore manifest outputs. Preflight detects output aliases without
creating directories/artifacts. Current normal invocation performs analysis;
production artifact generation remains in the active compiler plan.

`--new-project` is standalone, validates native filenames/parents, appends `.toml`
unless already present case-insensitively, and creates exclusively without
replacing files/symlinks. Failed writes/closes clean up only its own partial file.
The deterministic UTF-8/LF template has one `app` target, `sources = []`,
`source_dirs = ["src"]`, empty option collections and `build/app[.exe]` output.
It creates no directories/source files. Exits: 2 for usage/unknown selection,
1 for manifest/source/I/O failure, 0 for successful checks/creation or warnings.
Defaults/inheritance, target dependencies, imports, profiles, exclusions, package
fetching, watch/cache and parallel execution remain outside schema version 1.

## Testing strategy and migration

The [case-level migration ledger](../docs/test-migration.md) is authoritative for
suite dispositions, replacements, justified exceptions and failure evidence.
Prefer production CLI workflows, real Erlang files, exact artifacts/diagnostics,
separately built runtime consumers and OTP comparisons. Test count is inventory,
not a quality target; wrapping old unit executables in subprocesses is not migration.

The 2026-09-28 migration moved normal project behavior from 16 native suites to
CLI/workflow fixtures, moved frontend behavior to exact CLI/OTP projections and
900 bounded source mutations, and exercised runtime startup/contexts/dispatch/
copy/publication/pinning/teardown through a separately built consumer (32 repeats).
Useful behavior must move before deleting a suite, its source, registration and
unused helpers. Existing broader regression/differential fixtures remain required.

Retain API-only budgets, invalid handles, raw-stage ownership, injected allocation/
write/close/sink failures, LLVM-invalid IR, all-64-tag truth tables and mathematical
32/64-bit ABI boundary/layout checks when public workflows cannot reach them.
AST/raw-attribute lifetime tests await compatible frontend sanitizer evidence.
`lexer_dump`, `preprocessor_dump` and `parser_dump` are stage adapters, not end-to-end
proof; remove them only after migrating every dependent fixture.

Keep private backend success/failure checks until equivalent real CLI artifact,
link and execution coverage exists. Steps 24–27's source-driven lowering adapters
do not satisfy that retirement condition. Do not add product flags solely to expose
internals or confuse an independent oracle with another path through the product.
Remaining migration routes follow the active compiler plan:

| Compiler steps | Intended observable coverage |
| --- | --- |
| 20–27 | Source/type behavior plus retained invariants; implemented through 27. |
| 28–35 | Generated runtime consumers. |
| 36–39 | CLI artifacts, inspection and options. |
| 40–42 | Executable behavior, OTP comparisons and performance. |
| 43 | Cross-object inspection, explicitly separate from native execution. |
| 44–45 | Faults, lifetime and resource limits. |
| 46 | Published validation matrix. |

## Validation and open work

Historical evidence remains tied to its original host, reference and test inventory:

- Preprocessing completed on 2026-09-18 with focused native/live-OTP validation;
  Linux/Windows were unvalidated at that stage. Per-step evidence remains in its
  contract, including copied headers and explicit upstream-crash handling.
- Parser validation on macOS arm64, 2026-09-19: C++23 Debug 46/46 and full quality;
  historical C++26 Debug 46/46, sanitizer and compiler/runtime/full configurations
  as recorded in the parser matrix. Phase V had 37 Debug and four focused ASan
  tests. Toolchain: CMake 4.4.2, Apple Clang 21, Boost 1.92, live OTP 29.0.5;
  historical source baseline was `751f87b703fe5948607d08e82599ce644b772e76`.
  C++26 is historical evidence, not the current supported language baseline.
- Projects on macOS arm64, 2026-09-20: each step passed a fresh compiler+runtime
  Debug build and full Lizard/clang-tidy. Per-step CTest totals for steps 1–22:
  `46,47,48,49,50,51,52,53,54,55,56,57,58,58,59,60,61,62,62,63,64,64`.
  Homebrew dependency discovery follow-up also passed 58/58 and explicit/absent-root
  reconfiguration. Step 21 passed Debug/compiler-only/ASan+UBSan 64/64 and runtime-only
  with absent TOML; step 22 checked documented examples, macOS wrapper, help/template,
  TOML blocks and links. Native path/alias capabilities are recorded in the matrix.
- Windows x64 migration, 2026-09-28, OTP 29.1.1: baseline 93 tests, 78 passed/15 failed;
  migration inventory 75, initial Debug/Release 74/75 with parser-hardening failure;
  runtime ASan 15/15. These initial results did not establish a clean commit gate.
  Subsequent stack/frontend fixes closed Debug/quality failures; see the compiler
  history and migration ledger rather than rewriting the earlier result as passing.
- Latest compiler steps 24–27: fresh Windows x64 compiler+runtime Debug 80/80 each,
  zero skips, full Lizard/clang-tidy and formatting/whitespace checks. Commits:
  `c86f539`, `f09e7b4`, `5db13f0`, `4301401`, respectively. This adds Windows evidence
  without completing the Linux x86/ARM or every 32-bit/native platform obligation.

Full LLVM-linked Windows frontend ASan remains blocked by rpmalloc/CRT duplicate
symbols; runtime-only ASan works with its documented thunk configuration. Keep
lifetime coverage and record toolchain limitations rather than suppressing them.
Full upstream OTP suites, remaining native/cross platform matrix, generated-program
behavior and production artifact/link workflows are not declared complete here.
Future implementation commits still require the active plan's gates and AGENTS.md;
this archive replaces obsolete proposals, not those requirements.
