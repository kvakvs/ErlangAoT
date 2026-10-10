# Completed implementation work

Compact record of completed work, compatibility decisions and historical
evidence (consolidated 2026-09-30, compacted 2026-10-04, plan 11 phases F–M
added 2026-10-10). Current behavior lives
in the linked `docs/`; architecture and file ownership are in [arch.md](arch.md)
and [files.md](files.md). Full earlier wording is in Git history.

| Area | Delivered | Remaining qualification |
| --- | --- | --- |
| Foundations | CLI/CMake scaffold, 2026-09-17 | — |
| Preprocessor | Steps 1–13, 2026-09-18 | Focused OTP comparisons; no full upstream Common Test run. |
| Parser | Steps 1–18, 2026-09-19 | Platform matrix not closed. |
| Projects | Steps 1–22, 2026-09-20 | Historical macOS and later Windows evidence stay distinct. |
| Test migration | Frontend/project/runtime migrations, 2026-09-28 | Ledger audit (plan 11 step 70). |
| Compiler/runtime milestone | Steps 1–46, 2026-09-29 | Immediate-only subset and runtime skeleton. |
| Pattern matching and guards | Steps 1–20 and 15a, 2026-10-01–03 | Function clauses and body matches over the admitted domain. |
| Plan 11 | Steps 1–68 (phases A–L, M without 66/69/70), 2026-10-03–10 | Linked executables, ERTS-style heaps and collection, exceptions, recursion, comprehensions, records, funs, processes, multi-worker scheduling, ports, precise inference, specialization, tooling, Linux/x86/ARM validation and sanitizers. |

**Still unfinished:** macOS Apple Silicon validation, upstream OTP Common Test
evidence, the test-migration ledger audit, the optional scope decisions (D01–D07)
and final closure (plan 11 steps 66, 69–78).
See [outstanding work](#outstanding-work-to-finish) and [the backlog](01-todo.md).

## Foundations

Targets Erlang/OTP 29 on Windows x86-family, Linux x86/ARM and macOS Apple
Silicon. C++23, CMake 3.28+, warnings as errors, Clang reference toolchain.
`clau` builds the compiler; `clause_runtime` builds independently and never
depends on LLVM or compiler internals; `clause_abi` carries shared
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
matching runtime through `Clause::generated_program`.

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

## Plan 11 (steps 1–68, 2026-10-03–10)

Commits per step: [11-plan.md](11-plan.md#completed-steps); full step text in
Git. Contracts in the linked `docs/`. Windows x64 unless noted.

| Steps | Delivered |
| --- | --- |
| 1–2 (A) | `maint-29` pin `21776803`; fast/full test modes; changed-scope quality; six OTP-goldened program fixtures with a feature map. |
| 3–8 (B) | Entry/argv/exit contract ([executables](../docs/executables.md)), escript mode, `erlang:display/1`, startup object and `halt/0,1`, Clang linking of positional and project builds with staged publication, executable golden runner. Phase close: 138/138 CTests. |
| 8A–8I (C) | ERTS-style heap ([runtime-heap](../docs/runtime-heap.md)): off-heap binary list, parseable areas with walker, header admission, raw-word host terms, one heap block plus fragments, Cheney collector with ERTS sizing; per-context footprint 66 KB → 2.4 KB. |
| 9–16 (D) | `begin`/`case`/`if` with OTP scoping and `case_clause`/`if_clause`; `error`/`exit`/`throw`; `catch`; `try ... of ... catch` and `after`; stack traces (8 frames), `Class:Reason:Stack`, `raise/3`; `maybe` with `else_clause`. |
| 17–22 (E) | Execution model ([execution-model](../docs/execution-model.md)): explicit frames on a flat process stack, `musttail` transfers; recursive call graphs, proper tail calls, deep non-tail recursion; list, binary and map comprehensions with OTP generator/filter errors. |
| 23–28 (F) | Roots for frames, registers and the failure channel; safepoints and collection from generated code; `out_of_memory` without default cap, optional `--max-heap`/`--max-stack`/`--max-memory`; list/map caps removed, tuple arity and bignum limits as OTP; sharing-preserving copies between heaps. |
| 29–35 (G) | Record updates, `record_info/2`, local/qualified/anonymous native records (31A–31E); funs with and without captures, named funs, dynamic calls and `apply`. |
| 36–41 (H) | Production builtin bridge; term-access and conversion families; project-owned `lists`/`maps` subsets; `io:format`/`put_chars`; typed native callables. |
| 42–53 (I) | Pid/reference identities; cooperative executor, interruptible builtins (43A); exits and crash reports; signal inbox and send; selective receive and `after`; links, `trap_exit`, monitors, registered names; collection with mailboxes; identity guards; port scope decision (superseded by J2). |
| 54–57 (J) | Synchronized atom table and code server; several scheduler workers; cross-worker wakeups, timers, shutdown. |
| 57A–57G3 (J2) | Ports ([ports](../docs/ports.md)): identities and table, I/O poller, subprocess, file, standard I/O and socket ports, one event-driven I/O thread, port tasks on workers, busy-port suspension. |
| 58 (K) | Six program fixtures run through their manifests and match OTP at O0/O2 on 1 and 4 workers. |
| 58A–58N3 (L) | Inference ([semantic](../docs/semantic.md#inference)): fact domain, literal/operator/container/fun facts, caller inputs, narrowing by patterns, guards, uses and integer comparisons, entry/success domains, spec contradictions as errors, per-clause function types, call selection, per-call re-analysis, dependent facts of `case`/`if`/`try ... of`; `--print-types`. |
| 59–62B (L) | O2 proofs remove tag/shape checks; debug info (`-g`); profiling (`--profile`); LTO (`--lto`); hashed code-server lookups; timer wheel. Phase close: 235/235 full CTests, `check-quality-all` clean. |
| 63–65, 67–68 (M) | Linux x86-64 full gate (WSL2); 32-bit runtime on Windows x86 and Linux i386 with cross-linked goldens; arm64/armhf under qemu-user; ASan/UBSan/LSan and TSan clean on Linux after fixing a shutdown use-after-free and a lock-order inversion ([validation](../docs/validation.md)). |

<a id="outstanding-work-to-finish"></a>

## Outstanding work to finish

[The backlog](01-todo.md) expands each gap; [plan 11](11-plan.md) orders it.
Done: production executables, matching and guards, admitted terms, process
heap and collection, graph copying, the Erlang semantics above, identities
and atoms, processes, scheduling and ports, runtime services, tooling and
inference, sanitizers.

- [ ] **Native platform validation:** macOS Apple Silicon (step 66) and native
  ARM hardware; Linux x86-64, 32-bit x86 and ARM under emulation done.
- [ ] **OTP compatibility:** upstream Common Test suites and wider
  differential comparisons (step 69).
- [ ] **Test migration closure:** retire adapters only after equivalent public
  coverage (step 70).

Deferred, not required: dynamic modules (D01), atom collection (D02),
behavior-changing attributes (D03), public interchange (D04), stage readers
(D05), C/FFI (D06), project-schema extensions (D07). Every change keeps
formatting, behavioral tests and the fresh combined Debug quality gate without
weakened thresholds or suppressions.
