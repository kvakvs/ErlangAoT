# Completed implementation work

Compact record of completed work, compatibility decisions and historical
evidence (consolidated 2026-09-30, compacted 2026-10-04). Current behavior lives
in the linked `docs/`; architecture and file ownership are in [arch.md](arch.md)
and [files.md](files.md). Full earlier wording is in Git history.

| Area | Delivered | Remaining qualification |
| --- | --- | --- |
| Foundations | CLI/CMake scaffold, 2026-09-17 | — |
| Preprocessor | Steps 1–13, 2026-09-18 | Focused OTP comparisons; no full upstream Common Test run. |
| Parser | Steps 1–18, 2026-09-19 | Platform matrix not closed. |
| Projects | Steps 1–22, 2026-09-20 | Historical macOS and later Windows evidence stay distinct. |
| Test migration | Frontend/project/runtime migrations, 2026-09-28 | Frontend sanitizers pending. |
| Compiler/runtime milestone | Steps 1–46, 2026-09-29 | Immediate-only subset and runtime skeleton. |
| Pattern matching and guards | Steps 1–20 and 15a, 2026-10-01–03 | Function clauses and body matches over the admitted domain. |
| Plan 11 | Steps 1–10 and 8A–8I, 2026-10-03–05 | Linked executables, golden runner, ERTS-style heap with a copying collector on host request, `case`/`begin`/`if`. |

**Still unfinished:** GC and graph copying, process execution/messaging, more
Erlang source contexts and representations, native platform/sanitizer closure.
See [outstanding work](#outstanding-work-to-finish) and [the backlog](01-todo.md).

## Foundations

Targets Erlang/OTP 29 on Windows x86-family, Linux x86/ARM and macOS Apple
Silicon. C++23, CMake 3.28+, warnings as errors, Clang reference toolchain.
`erlang_aot` builds `erlangaot`; `erlang_runtime` builds independently and never
depends on LLVM or compiler internals; `erlang_aot_abi` carries shared
contracts. Stages exchange owned internal data; only directory locations are
reserved for `compiler/src/stage_readers/{preprocessed,abstract,ir}/`. APIs stay
project-internal C++23 (earlier C wrappers and C++26 options were superseded).
Diagnostics keep source/macro/include context on stderr; failure is the exit
status.

## Preprocessor

Contract and limits: [preprocessor.md](../docs/preprocessor.md). Independent of
LLVM/runtime, one isolated session per source; Boost.Parser is a pinned
character-parser dependency (a token-iterator adapter was rejected).

| Steps | Delivered |
| --- | --- |
| 1–3 | Parser/oracle boundary, source buffers/encoding/locations, Erlang lexer, form dispatch and recovery. |
| 4–6 | Object/parameterized macros, overloads, definition order, recursion checks, rescanning, stringification from original tokens. |
| 7–9 | Nested conditionals, include/include_lib lookup, contextual module/file/line/function/machine/OTP macros. |
| 10–11 | Restricted condition evaluator; feature configuration and feature-query macros. |
| 12–13 | Warning/error directives, failure latching, provenance, CLI options, OTP golden fixtures. |

Preserved OTP details: only `true` selects a branch (`42` is false), evaluation
failure is false while malformed syntax diagnoses, skipped regions still check
structure, fun types need no `end`, `maybe`/`else` change token classes when
enabled; `maybe_expr` is approved/default-on, `compr_assign` experimental/off.
Reference: `lib/stdlib/src/epp.erl`, `epp_SUITE.erl` (cases `predef_mac`,
`rec_1`, `overload_mac`, `otp_8130`, `stringify`, `test_if`, `include_local`,
`file_macro`, `function_macro`, `encoding`, `test_error`, `scan_file`),
`erlc_SUITE` `features_*`, `compile_SUITE` `cond_and_ifdef`, `erl_scan_SUITE`
sigils/triple-quoted strings. Full upstream execution needs a matching built OTP
with its Common Test hooks.

## Parser

Contract and evidence: [parser.md](../docs/parser.md). Consumes expanded tokens
directly (no relexing). Move-only module arenas with checked expression,
pattern, type and form IDs; source metadata keeps logical anchors and physical
origins across macros/includes; ASTs outlive preprocessing sessions. Parsing is
transactional per form and bounded in nodes, depth, work and diagnostics.
Syntax acceptance is separate from binding, guard legality, linting and types;
record expansion and `v3_core` are not parsing.

| Steps | Delivered |
| --- | --- |
| 1–4 | Grammar/oracle inventory, shared token mechanics, AST ownership, transactional forms. |
| 5–9 | Literals, operators/calls, clauses, patterns, guards, maps, records, binaries. |
| 10–12 | Blocks, branching, receive, funs, try/catch, maybe, comprehensions. |
| 13–15 | Attributes, type/opaque/nominal declarations, specs/callbacks. |
| 16–18 | Recovery, `--parse-check`/`--print-pp`/`--print-ast`, differential coverage, pinned corpus. |

Closure: 344 grammar productions witnessed (423 inventory rows), 43 positive and
183 negative fixtures, three OTP builder exceptions; a ten-source OTP corpus
(hashes in `tests/fixtures/parser/phase6/otp.tsv`) proves syntax coverage only.

## Projects

Usage and schema: [projects.md](../docs/projects.md). Private code owns toml++
3.4.0 loading, schema, paths/globs, selection, planning, CLI and creation.
Schema 1: integer `schema_version = 1`, unique target names
`[A-Za-z0-9_][A-Za-z0-9_.-]*`, nonempty `sources` or `source_dirs`; unknown keys,
wrong types and duplicates fail everywhere. Repeated `--target` selects in
request order; unknown selectors exit 2. Manifest paths resolve against the
manifest's directory, CLI paths against cwd. Globs support `*`, `?`, `**` only;
directory discovery skips symlinks. Sources keep entry order, sort each
expansion by UTF-8 bytes and deduplicate by physical identity. CLI include paths
precede manifest ones. `--new-project` creates a template exclusively (one `app`
target, `source_dirs = ["src"]`, `build/app[.exe]` output). Since plan 11 steps
6A–7, project targets with `output` or `entry` link executables. Defaults,
dependencies, imports, profiles, exclusions, fetching, watch/cache and parallel
execution stay outside schema 1 (D07).

## Testing strategy and migration

The [case-level ledger](../docs/validation.md#test-design) is authoritative.
Prefer CLI workflows on real Erlang files, exact artifacts/diagnostics, separately
built runtime consumers and OTP comparisons; test count is not a goal. The
2026-09-28 migration moved 16 native project suites to CLI fixtures, frontend
behavior to CLI/OTP projections and 900 bounded mutations, and runtime behavior
to a separately built consumer. Keep API-only budgets, invalid handles, injected
allocation/write failures, invalid IR and 32/64-bit layout checks that public
workflows cannot reach. Stage dump adapters and private backend checks stay
until equivalent public coverage exists.

## Compiler and runtime milestone (steps 1–46, 2026-09-29)

Contracts: [compilation](../docs/compile.md), [semantics](../docs/semantic.md),
[specialization](../docs/specialization.md), [features](../docs/features.md).

| Steps | Delivered |
| --- | --- |
| 1–8 | LLVM SDK discovery, owned compilation results, target policy, verification, object emission, ABI v1, feature reporting. |
| 9–14 | Runtime/context lifecycle, immediate terms, frozen builtin registries, lazy memory, scheduler bookkeeping, explicit unavailable services. |
| 15–23 | Declarations/exports, subset checks, bindings, acyclic batch calls, symbolic declared types, independent inference, contract warnings. |
| 24–28 | Target-width integers, parameters, local/remote calls, versioned descriptors, transactional registration with native execution. |
| 29–32 | Bounded specialization with guarded variants, LLVM O0/O2, text/bitcode serialization. |
| 33–39 | Artifact publication, options, positional/project integration, `--verbose`, IR and type inspection. |
| 40–46 | Native harnesses, OTP comparisons, specialization costs/caps, cross-target inspection, failure hardening, examples. |

Lasting decisions: unsupported syntax is rejected even in unused functions;
specs warn on contradictions but never narrow representation; `-O0` stays
generic, `-O2` adds bounded variants (3 per function, 32 per module, 128 per
target, 2x growth) with generic fallback; `--no-type-specialization` always
wins. `--emit obj|llvm-ir|llvm-bc`, `--artifact-dir`, `--target-triple`,
`--print-ir`, `--print-optimized-ir`, `--print-types`. Failed compilation keeps
earlier outputs; multi-file replacement is not atomic. ABI v1: unsigned target
words, checked 28/60-bit small integers, collision-free symbols, generic entries
take a context and aligned term array. Every runnable program links one
matching runtime through `ErlangAoT::generated_program`.

Validation: macOS arm64 for steps 1–14 (65 to 93 CTests), Windows x64 for steps
15–46 (76 to 103 CTests, up to 182 quality commands), Clang/SDK 23.1.2,
clang-tidy 22.1.8. Runtime-only ASan passed; full compiler ASan was unavailable
(SDK STL annotation ABI mismatch) and is not counted. Per-step tables are in Git
history and [validation](../docs/validation.md#history).

<a id="completed-patternmatch"></a>

## Pattern matching and guards (steps 1–20 and 15a, 2026-10-01–03, Windows x64)

Context: [retained plan summary](11-plan.md#completed-patternmatch),
[semantic matrix](../docs/patterns.md), [guards](../docs/guards.md). Executes
ordered function clauses with guards, body matches/sequences and acyclic
local/exported remote calls over owned atoms, arbitrary integers, finite floats,
tuples, lists, strings, exact-key maps, bitstrings and ordinary tuple records.

| Steps | Delivered |
| --- | --- |
| 1–3 | Semantic matrix (81 signatures), revision-2 checked failure channel, runtime atoms and revision-3 module bindings. |
| 4–6 | Clause-local bindings and facts, pattern normalization, immediate matching with shared equality. |
| 7–10 | Guard resolution and services, grouped/strict/lazy control flow, ordered clause candidates, RHS-first body matches. |
| 11–12 | Stable heaps, revision-4 generated roots and handoffs; rooted tuple/list/string construction and matching. |
| 13–17 | Arbitrary integers, binary64 floats, exact-key maps, project-owned goldens (15a), bitstrings, ordinary records. |
| 18–20 | Audited guard catalog (77 of 81 rows; `self/0`, `node/0,1`, native `is_record/1` gated), binding facts, stress and closure. |

Final: 19 owned corpora with 67,634 expected outcomes plus 106 semantic rows,
1,082,144 golden comparisons over eight driver/policy combinations; fresh
Windows x64 Debug 124/124 CTests and 258 quality units. Descriptors use ABI
revision 4; the revision-2 failure channel is unchanged. Normal builds and tests
need no OTP.

## Plan 11 (steps 1–17, 2026-10-03–05, Windows x64)

Compact per-step record: [11-plan.md](11-plan.md#step-1).

- **Baseline (1–2):** `maint-29` unchanged at `21776803`; fast/full test modes;
  changed-scope quality and formatting; `docs/` reduced to 15 notes; six
  OTP-goldened program fixtures with a feature map.
- **Executables (3–8):** entry/argv/exit contract (`docs/executables.md`),
  escript mode, `~w`/`erlang:display/1` printing, startup object
  (`erlang_aot_main_v1`) and `erlang:halt/0,1`, Clang linking for positional
  and project builds with staged all-or-nothing publication, and the executable
  golden runner (`tests/fixtures/executables/`). Phase B close: full 138/138
  CTests, 272 quality units.
- **Classic heap (8A–8I, phase C closed):** contract `docs/runtime-heap.md`; heap and off-heap
  binary cells with a per-process off-heap list; parseable areas with walker and
  `verify()`; admission by owned range and header shape (no object index);
  host `Term` as raw word with lifetime and collection count; segmented process
  root stack; one heap block per process plus heap fragments; full-sweep Cheney
  collector on explicit host request with ERTS sizing. Per-context footprint
  fell from 66 KB to 2.4 KB; per-cell side metadata from 24 MB to none after a
  collection (`docs/runtime-heap.md#measurements`).
- **Control flow (9):** `begin`/`end` and `case` with ordered clauses, guards,
  OTP export/unsafe scoping and `{case_clause, V}`; executable goldens
  `case_select`, `case_scope`; OTP-classed binding rows in the bindings corpus.
- **`if` (10):** guard-only clauses share the `case` scoping and joins;
  exhaustion raises `if_clause`; golden `if_select`, four `if_*` binding rows.
- **Raise (11):** `error/1,2,3`, `exit/1`, `throw/1` (qualified or auto-imported)
  raise through `erlang_aot_raise_v2` with `raised_*` reasons; startup reports
  `uncaught exception <class>: <reason>`; golden `raise_classes`.
- **`catch Expr` (12):** failures inside reach a handler that calls
  `erlang_aot_catch_v1` (thrown term, `{'EXIT', R}`, `{'EXIT', {R, []}}`);
  halts and runtime failures pass through; inner bindings unsafe afterwards;
  golden `catch_values`, four `catch_*` binding rows; `nowarn_*` compile options.
- **`try ... of ... catch` (13):** the body's handler takes `{Class, Reason}`
  via `erlang_aot_exception_v1`; catch clauses match class (default `throw`),
  reason and guard; unmatched re-raise via `erlang_aot_reraise_v1`; `of` clauses
  raise `{try_clause, V}`; everything bound inside is unsafe afterwards; golden
  `try_catch`, six `try_*` binding rows. Catch class/stacktrace are AST
  expressions.
- **`try ... after` (14):** an after protection encloses body and clauses; the
  normal path runs the after body once, the after handler takes the exception,
  runs a second copy and re-raises; golden `try_after`, native
  `codegen_after_fault_O0/O2` (budget failure in the after body), three
  `try_after*` binding rows.
- **Stack traces and `erlang:raise/3` (15):** root frames carry a
  `FrameDescriptor` (`erlang_aot_roots_enter_v5`); an Erlang exception copies
  the innermost 8 named frames, built into `[{M, F, Arity, []}]` only for
  `catch`, handlers and reports; `Class:Reason:Stack` binds it
  (`erlang_aot_exception_v2`), re-raise keeps it (`erlang_aot_reraise_v2`, also
  `raise/3` with BEAM's stack validation and `badarg` result), `error/2,3`
  show their argument list (`erlang_aot_error_v1`); `stacktrace_bound`/
  `stacktrace_guard` lint, `get_stacktrace/0` rejected; golden `stack_traces`,
  three `try_stack*` binding rows. The `exceptions` capability is implemented.
- **`maybe` (16):** each `?=` is a match whose mismatch edge leaves for the
  maybe's exit with the unmatched value; without `else` that value is the
  result, otherwise `else` clauses select like `case` clauses and raise
  `{else_clause, V}` (`ErrorReason::else_clause = 15`); bindings follow OTP
  (nothing exported, `else` sees body names unsafe); golden `maybe_else`, four
  `maybe_*` binding rows, feature-disabled source rejected. The `pattern
  matching` capability is implemented.
- **Execution model decision (17):** `docs/execution-model.md` fixes explicit
  frames on a flat moving process stack, X-register arguments, entry plus
  resume-switch body per function, `musttail` transfers (trampoline
  fallback), entry reductions for yield and unwinding to handler frames;
  prototype `tests/prototypes/execution_model/` compared native calls and
  LLVM coroutines and checked all required targets.
- **List comprehensions (21):** every generator is a loop in the function body
  with its cursor and the reversed accumulator in frame term slots (constant
  stack), reversed once by `ContainerConstruction::reverse`; generator patterns
  shadow, relaxed ones skip, strict ones raise `{badmatch, E}`, zip groups run
  in step and raise `{bad_generators, Inputs}`, non-lists `{bad_generator, T}`;
  guard-test filters reject like guards, others raise `{bad_filter, V}`
  (`ErrorReason` 16-18); golden `list_comprehensions` (100k-element inputs).
- **Binary and map comprehensions (22):** bitstring generators match a prefix
  (plan `GeneratorPattern::element` adds a tail segment) and skip rejected
  elements with OTP's skip pattern (`skip`: values ignored, floats as
  integers); map generators walk `key_at`/`value_at` positions; producers
  accumulate like lists and finish with `BitOperation::concat` or
  `MapOperation::from_list`; zip payloads show OTP's map iterator chain
  (`MapOperation::iterator`). Golden `bit_map_comprehensions` covers every
  generator/producer combination. Phase E closed.

<a id="outstanding-work-to-finish"></a>

## Outstanding work to finish

[The backlog](01-todo.md) expands each gap; [plan 11](11-plan.md) orders it.

- [x] **Production executables:** startup, entry policy and Clang linking for
  positional and project builds; golden runner (plan 11 steps 3–8).
- [x] **Scoped matching and guards:** ordered clauses, body matches, bindings,
  grouped/strict/lazy guards and the admitted service catalog.
- [x] **Admitted terms:** atoms, integers, floats, tuples/lists/strings, maps,
  bitstrings and ordinary records with checked construction, access,
  comparison and fault cleanup.
- [x] **Parseable process heap:** ERTS word layout, off-heap binaries, header
  admission, raw-word host terms, root stack, heap block plus fragments,
  copying collector on host request (8A–8I).
- [ ] **Collection and copying:** generated-code safepoints, graph copying
  between heaps, mailbox/transit roots (host-requested copying collection done
  in 8H; frame, register and failure-channel roots enumerated in plan 11
  step 23).
- [x] **More Erlang semantics:** maybe, comprehensions, exceptions and
  handlers, recursion and tail calls (plan 11 phases D and E); record updates,
  `record_info/2` and native records (steps 29–31E); function values,
  closures, named funs and dynamic calls (steps 32–35; builtins as values and
  dynamic calls of builtins wait for the bridge, step 36).
- [ ] **Identities and atoms:** pid/port/reference services; synchronized atom
  access before workers; atom collection is a scope decision (D02).
- [ ] **Processes and scheduling:** cooperative execution, reductions, workers,
  signals, send and selective receive (all messages enter the signal inbox).
- [ ] **Runtime services:** generic production builtin registration and
  families, typed callables, concurrent code server; dynamic loading is D01.
- [ ] **Tooling:** source-driven specialization, debug info, profiling, LTO.
- [ ] **Native platform validation:** Linux x86/x64/ARM/AArch64, macOS Apple
  Silicon, Windows x86.
- [ ] **Sanitizers and compatibility:** compiler/frontend ASan/UBSan/LSan;
  upstream OTP Common Test suites.
- [ ] **Test migration closure:** retire adapters only after equivalent public
  coverage.

Deferred, not required: public interchange (D04), stage readers (D05), C/FFI
(D06), project-schema extensions (D07). Every change keeps formatting,
behavioral tests and the fresh combined Debug quality gate without weakened
thresholds or suppressions.
