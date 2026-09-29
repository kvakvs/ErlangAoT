# LLVM compilation integration plan

Status: steps 1-37 complete, 2026-09-29. Steps 38-46 remain pending.
Execute the numbered steps individually, each with passing validation and its own commit.

## Objective and current boundary

Compile a small Erlang/OTP 29 subset to verified LLVM IR, bitcode and native
objects. Prove execution with a Clang-linked C++ harness and the mandatory
`erlang_runtime` library. This milestone builds the runtime skeleton; full Erlang
behavior and a production executable launcher remain later work.

The preprocessor supplies expanded tokens to a parser owning a move-only
`ast::Module`. Parsing does not establish semantic validity. The driver's
`compiler/src/driver/frontend.cpp` validates declarations, parameter bindings, batch calls and declared types,
then infers implementation facts and checks declared contracts conservatively
after parsing; positional and project compilation still write no executable. Frontend
check/print actions and `[pp]`/`[parse]` tracing work. The runtime is a static library
with feature reporting, lifecycle, immediate terms and generic native dispatch.
Immediate-only host Terms, builtin dispatch and memory service boundaries are implemented;
TermFactory reporting placeholders are installed; term creation, backing allocation and collection remain pending.
`erlang_aot_abi` supplies versioned term/context/function headers and checked
immediate integer encoding; global LLVM SDK discovery/linkage, target
setup, verification and synthetic object emission are implemented. Private compilation
owners retain batch ASTs, LLVM state and results. Generic lowering, runtime module
registration, bounded specialization planning and guarded variant lowering are
implemented privately. Backend driver integration remains deferred. The selected global LLVM installation is recorded in `docs/compile.md`.

## Runtime API sketches to build upon

Review headers in `runtime/include/` and notes in `runtime/design/` define evolving
service/ownership proposals, not completed plan steps. CMake lists the prototype
headers for IDE navigation. `src/runtime.cpp`, `src/process/`
and `src/diagnostics/features.cpp` implement lifecycle and feature reporting.
Implemented host APIs live in `include/erlang_aot/runtime/`; extend these APIs and
update this inventory and affected steps together when sketches change.

| Header under `runtime/include/` | Sketch                                                                                           | Steps         |
| ------------------------------- | ------------------------------------------------------------------------------------------------ | ------------- |
| `base_types.hpp`                | Forwards implemented target word/tag definitions to namespaced runtime API                                                        | 7, 10, 12     |
| `terms.hpp`                     | Immediate Term API forwarded; TermFactory lifetime binding and reporting placeholders         | 7, 9, 10, 12, 14  |
| `../src/terms/term_layout.hpp`   | Private slots, headers, heap layouts, Multiprecision `Bignum`, assertions; not a public/wire ABI | 7, 10, 12     |
| `atom_storage.hpp`              | Runtime-wide stable atom IDs, lookup, options/statistics, collection placeholder                 | 9, 10, 14, 28 |
| `process_heap.hpp`              | Lazy owner/accounting, checked allocation/collection rejection and immediate addition implemented; graph storage reserved | 9, 12 |
| `binary_heap_object.hpp`        | Shared immutable word vector sketch; zero tail means full words, otherwise valid high bits       | 12            |
| `process.hpp`                   | Shared state enums implemented; reductions, cooperative code, owned signals/inbox reserved       | 9, 12, 13     |
| `mailbox.hpp`                   | Empty owner implemented; selective cursor, reads and signal handling reserved                    | 12, 13        |
| `scheduler.hpp`                 | Includes implemented SchedulerService; worker/pool options, replies and signal handling reserved | 9, 13, 14     |
| `callable.hpp`                  | Implemented generic Callable/results/keys/registry forwarding header; typed proposals unverified                    | 11, 28        |
| `unverified/native_callable.hpp.txt` | `NativeCallable<Args...>` alias for `TypedCallable<Args...>`                                     | 11, 28        |
| `code_server.hpp`               | Code images, immutable loaded modules, pinned generic calls, runtime-wide server                 | 9, 11, 28     |

Supporting contracts: [terms](../runtime/design/terms.md),
[processes/schedulers](../runtime/design/processes.md),
[atoms](../runtime/design/atom_storage.md), and
[module registries](../runtime/design/code_server.md).
ABI v1 now fixes immediate low tags and target-word encoding. The private tag/header
are one word with mask/shift decoding and compile-checked fixed heap prefixes;
header content counts exclude its own word. Heap construction/allocation remains deferred.
`TermTag::get_kind()` uses a constexpr three-level lookup; boxed kinds still need
header inspection. Immediate identities resolve to `local_pid`/`local_port`, empty
containers to `empty_tuple`/`empty_list`. CTest `runtime_term_tag` checks all 64 tag
combinations in `tests/runtime/term_tag.cpp`; the build-only object target
`runtime_term_layout_tests` compiles private prefix assertions and immediate ABI agreement.

Step 9 adds `erlang_aot/runtime/{runtime,process_context}.hpp`, with stable owned
contexts, non-recycled identities and lifetime-token invalidation before mailbox/heap
teardown. The runtime-wide code server is implemented by step 11; atom ownership and
signal admission remain deferred. The C compatibility layer added in steps 7–9
was subsequently removed at user request; the C++ Runtime API is the sole lifecycle
interface, with scoped `Status` and constexpr ABI constants. [Lifecycle contract](../docs/runtime-lifecycle.md)
documents status reporting, host serialization and the mandatory
`ErlangAoT::generated_program` link target.

Step 10 adds [immediate word services](../docs/runtime-terms.md) in
`erlang_aot/runtime/terms.hpp`: checked structural classification and native integer
encoding/decoding using ABI v1. Shared word/tag/error declarations moved into the
namespaced public headers; step 11 implements immediate-only Term values, while step 14 adds TermFactory reporting placeholders.
Heap structs moved to `runtime/src/terms/term_layout.hpp`, visible only to runtime
internals and the focused layout test. Atom/pid/port tag recognition is structural,
not identity validation. Header/catch and malformed empty encodings fail; heap tags
are rejected without dereferencing. No host ownership or atom table is fabricated.

Step 11 implements [generic builtin dispatch](../docs/runtime-builtins.md), using one
frozen registry per module and one runtime-owned code server. Generic keys contain
exact name/arity and all-Term type sequences. Typed/native sketches now live under
`runtime/include/unverified/` and remain deferred. Immediate-only host Terms support
calls without roots; heap/identity values are rejected. Owned string names are copied from generated descriptors in step 28; atom
initialization remains reserved. The generated service bridge returns Status separately from the
output word and reports missing/unavailable BIFs once. Publication/lookup remain
host-serialized; unload and concurrent workers remain deferred. Top-level callable
and code-server headers now forward to the namespaced implementation.

Step 12 adds [process memory ownership](../docs/runtime-memory.md) under
`runtime/src/memory/`: lazy heap lifecycle, byte-budget validation, word-size/limit
checks and explicit unavailable allocation/collection. `Term::copy_to` and heap
`add` revalidate owner-independent immediates only; they need no roots and may cross
runtimes. Heap graph copying, root registration, safe points and binary allocation
remain deferred. Future mailbox/cursor roots and owned signal transit must be
included before heap Terms are admitted; C++ cell resources require explicit
construction/destruction rather than byte relocation.

Step 13 adds [scheduler lifecycle bookkeeping](../docs/runtime-scheduler.md) in
`erlang_aot/runtime/{process_state,scheduler}.hpp` and `runtime/src/scheduler/`.
Each runtime owns one SchedulerService; explicit once-only registration references
existing context identities without taking context ownership. Checked dispatch
boundaries, suspension, returns and removal change metadata only. Runtime teardown
clears records before contexts and code; destroying a running registration's context
returns busy. Worker execution, reduction grants, wake/signal handling and receive
remain reserved in the original process/pool/mailbox sketches.

Step 14 adds [runtime service placeholders](../docs/runtime-services.md): context-bound
TermFactory failures, catalog reports at allocation/collection, send, worker/execution
and unload boundaries, and exact known-deferred BIF identification. Unknown BIFs
return a distinct status. Diagnostic failures propagate, successful lifecycle stays
silent, and no runtime semantics are fabricated. Atom collection, dynamic file loading
and generated descriptors remain reservations until their owners/ABIs exist.

Carry these ownership and service contracts into implementation:

- `ProcessContext` owns its heap/mailbox. `Term::copy_to` and `ProcessHeap::add`
  explicitly copy owned graphs; future GC traces host, continuation, mailbox and
  receive-candidate roots. Emitted widths come from target data layout, not host `Word`.
- `BinaryHeapObject::create` returns shared ownership of immutable `std::vector<Word>`
  storage. Counts must exceed `HEAP_BINARY_THRESHOLD_WORDS` (64 / sizeof(Word));
  otherwise return `BinaryHeapObjectError::invalid_size`. Nonzero tails count valid
  high bits in the last word, including byte-aligned tails; zero means full words.
  Last-owner destruction releases storage directly, without a pool or owner callback.
- One `AtomStorage` per runtime provides stable, non-recycled IDs with dense indexing
  and name lookup; default/hard caps are 2^20/2^26, collection a placeholder. Emit
  atom spellings/slots, never compiler-assigned IDs. Initialize read-only bindings
  through runtime calls before publication and retain roots for loaded-code lifetime.
  This metadata support does not enable atom-valued source expressions.
- Cooperative continuations use `ReductionBudget`/`StepResult`. All messages,
  including self-send, enter the recipient's signal inbox as `ProcessSignal`.
  Bounded owner-worker safe-point handling copies payloads to its heap and invokes
  `Mailbox::append_handled_message`; enqueueing neither inserts nor resumes code.
  Service waiting/suspended processes without clearing explicit suspension. Receive
  cursors retain unmatched messages/position, remove only a match and handshake at
  the tail. Sends acknowledge local acceptance; scheduler replies acknowledge handling.
  Preserve per-sender ordering across signal kinds.
- One runtime-wide `CodeServer` publishes uniquely owned, frozen per-module registries.
  Keys include function name, arity and exact argument types. Generic calls use
  all-Term spans; typed calls use declared values without a conversion registry.
  Generic fallback and `CallResult<Term>` construction are explicit. `ResolvedFunction`
  pins its module; direct/typed views require a retained handle through invocation
  and target destruction. Unload removes lookup access while handles retain the
  registry, atom bindings and code image.
- STL/std::function/RTTI interfaces are host-side C++, not the generated-code ABI.
  Native calls are bounded and synchronous; the old virtual call-frame protocol is
  dropped. Conversion helpers, cooperative generated calls, worker execution,
  messaging, allocation and GC remain beyond this skeleton.

## Responsibility split

ErlangAoT is a language frontend to LLVM, using its X86, ARM and AArch64 backends.
The project owns Erlang semantics, binding, types, lowering, the shared ABI and
runtime behavior. LLVM owns generic optimization, instruction selection, register
allocation and object emission; Clang/platform linkers own native linking.
Use SDK APIs and standard passes, not a new machine target, optimizer, assembler,
object writer or linker. `llvm-link` does not link native executables.

LLVM verification proves IR consistency, not Erlang correctness. Define atom/term
semantics, clauses, guards and exceptions before lowering; justify every `nsw`,
`nuw`, `inbounds`, alias, memory-effect or exception attribute. Future integer
arithmetic needs correct bignum fallbacks, not machine wrapping or runtime use of
compiler-side `APInt`. LLVM GC/coroutine mechanisms do not supply an Erlang
collector, scheduler, isolation or bounded-stack tail recursion; its example GC
named `erlang` is not OTP. References: [object emission](https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl08.html),
[Clang linking](https://clang.llvm.org/docs/Toolchain.html),
[IR contracts](https://llvm.org/docs/LangRef.html), [GC](https://llvm.org/docs/GarbageCollection.html).

## Recommended first milestone

### Language scope and lowering

Accept named modules, exports and single-clause functions with distinct variable
or wildcard parameters. Bodies contain one expression: tagged-small-integer
literals, parameter references, or direct local/literal `module:function(...)`
calls within the compilation batch, including nested arguments. Require an acyclic
call graph and declared exports for remote calls; diagnose unknown calls and
local/cross-module recursion. Separately linkable acceptance examples:

```erlang
-module(answer).
-export([value/0, identity/1]).
value() -> 42.
identity(X) -> X.
```

```erlang
-module(client).
-export([value/0]).
value() -> answer:identity(answer:value()).
```

Reject unsupported syntax even in unused functions: bignums, arithmetic, heap
terms, atom expressions, other patterns, guards, multiple clauses, closures,
dynamic calls, exceptions, receive, concurrency and code loading. Recognize
negative integer literals explicitly without enabling general unary arithmetic.
Handle file/module/export and type/spec attributes explicitly; allowlist inert
metadata and reject other attributes, including behavior-changing compile options,
parse transforms, `on_load` and parameterized modules. Syntax-only checking keeps
its broader coverage; describe compilation as a subset.

Keep binding, resolution, type and capability results in side tables beside the
immutable AST. Pipeline: declarations/bindings/calls → declared types → inference
→ generic lowering → optional specialization → LLVM optimization/emission.
Lower with `IRBuilder`; defer a custom IR until a concrete Erlang transformation
needs it. Do not add SSA/MLIR infrastructure, Core Erlang/BEAM readers or custom
LLVM passes. Follow [LLVM frontend guidance](https://llvm.org/docs/Frontend/PerformanceTips.html).

### Private ABI and runtime skeleton

Define a versioned contract in `abi/`: unsigned target-word terms, explicit checked
unsigned encoding of immediate signed integers, exact tags/alignment, reversible
collision-free module/function/arity symbols and export/lifetime rules. Use the
native free-function machine convention with a live project context pointer, argument-array
pointer and term result; resolved identity carries arity and direct calls retain
the context. Project callers supply valid terms. Prove C++ harness agreement on
each native platform; promise neither BEAM/general FFI compatibility nor future
tail-recursion support. GC, exceptions and suspension may revise this ABI.

Build `erlang_runtime` separately as C++23, initially static and LLVM-free. Shared
`cmake/BoostDependencies.cmake` provides Multiprecision to compiler/runtime and
runtime consumers. Runtime-only builds require Boost >=1.90, not Boost.Parser,
TOML or OTP; this wiring does not implement bignums. Generated service boundaries
use C++ APIs, scoped status enums and explicit error results. Host APIs use
`std::expected` and RAII; generated entries retain simple word/pointer signatures
without STL values or C++ exceptions crossing them. All APIs are project-internal
C++23. C-compatible headers/linkage are deferred until an actual external use arises.

| Runtime location                                 | Skeleton responsibility                                                           |
| ------------------------------------------------ | --------------------------------------------------------------------------------- |
| `include/erlang_aot/runtime/`, `src/runtime.cpp` | Explicit startup/shutdown and host API                                            |
| `src/process/`                                   | Isolated contexts and ownership; reserve reductions, signals, mailbox, exceptions |
| `src/terms/`                                     | Immediate-term inspection; extend the opaque Term sketch for later values         |
| `src/builtins/`                                  | Module-owned registries, all-Term defaults, unavailable-BIF errors                |
| `src/memory/`                                    | Memory ownership/lifecycle; reserve allocation, roots and GC                      |
| `src/scheduler/`                                 | Process registration/lifecycle; reserve queues, reductions, signals and wakeups   |
| `src/modules/`                                   | ABI-checked descriptors, frozen registries, code lifetime and initialization      |

Implement useful lifecycle, immediate inspection and module registration; reserve
later services without fabricated successful results. Extend the term sketch's
private word-aligned layouts/slots and synchronize API/layout notes. The harness
explicitly initializes runtime state, registers ABI/word-width-compatible modules,
creates live contexts and tears contexts down before runtime-wide services.

Every runnable generated-program link must use the matching runtime through a
reusable CMake interface target carrying runtime/platform dependencies, never the
host LLVM SDK or test replacement symbols. Objects/IR/bitcode retain registration
and ABI references; link one runtime per program, not per module. Compiler-only
builds can emit objects; foreign-target linking requires a separately built target
runtime. Clang performs harness linking; production startup/linking stays deferred.

### Types and bounded specialization

Consume `ast/types.hpp` and `ast/forms.hpp` exhaustively under
[OTP's type/spec contract](https://www.erlang.org/doc/system/typespec.html).
Support `-type`, `-opaque`, `-nominal`, `-export_type`, `-spec`, `-callback`, aliases,
parameters, remote references, overloads and `when` constraints, preserving identity
and visibility. Represent all structural categories symbolically when needed;
annotation support does not expand executable syntax. Resolve batch-exported types,
diagnose malformed/duplicate/undefined locals and treat unavailable external metadata
as diagnosed unknowns. Memoize recursive type graphs; executable recursion stays excluded.

Inference uses `term()`/`none()` as top/bottom, singleton/category facts, bounded
ranges/unions and parameter/result relations. Unknown is top. Bound expansion/work,
widen conservatively and diagnose work-budget exhaustion. Analyze exported inputs
as arbitrary valid terms; specs are contracts, not guards or representation proofs.
Warn on provable contradictions without rejecting valid dynamic behavior solely
for a narrow spec. Propagate freshly instantiated summaries in acyclic dependency
order, including remote calls, independently per project target. Infer `42` as a
singleton and identity as an argument/result relation. Wrong or missing specs must
not alter execution. This is not full [Dialyzer](https://www.erlang.org/doc/apps/dialyzer/dialyzer.html);
OTP comparison tools are not inference dependencies.

Keep a generic tagged-ABI body for every function. Default `-O0` performs analysis
but no specialization; `-O2` enables LLVM O2 and useful bounded type variants.
`--no-type-specialization` overrides either policy regardless of option order.

- Canonical candidates come from proven call-site argument profiles, never Cartesian
  union products or one clone per literal/context. Keep unrelated arguments generic;
  deduplicate/rank deterministically and prevent recursive callee cloning.
- Cap variants at 3/function, 32/module and 128/target, plus generic bodies. Bound
  candidate analysis and pre-LLVM IR growth, including dispatch, to 2x the generic
  baseline per function/module. Over-budget candidates use generic code, not errors.
- Require removal of actual dynamic operations, checks, conversions or dispatch.
  Constants/identity may need no variants. Prefer ordinary LLVM optimization; reuse
  [cloning utilities](https://llvm.org/doxygen/Cloning_8h.html) or the same lowering
  visitor for remaining benefits, then apply the verified standard pipeline.
- Specialize only implemented representations/guards: `integer()` is not proof of
  a small integer. Select directly with proof, otherwise use bounded safe tests and
  generic fallback. Unboxing/assumptions require dominating proofs, never specs alone.
  Preserve evaluation order, errors, overflow, context and future allocation/root rules.
- Report accepted/skipped profiles and reasons in `[comp]`; measure compile time,
  IR/object growth and execution to tune policy, without noisy timing test gates.

### Future-feature placeholders and diagnostics

Maintain one shared feature catalog mapping IDs to owner/boundary, status and
focused failure tests. Cover deferred semantic/lowering operations, term/BIF
services, processes/scheduling, allocation/GC, dynamic modules and future driver
linking. Place handlers only at real extension points and reference the plan/step
or explain the remaining work in TODOs; do not prebuild unused subsystems/readers.

A reached placeholder reports `[feature name] notimpl` once at its owning boundary
on stderr, independently of verbosity, with available source/module/target/operation
context. For example: `[pattern matching] notimpl: src/example.erl:12:5`.
Known unsupported source fails capability analysis before publication; defensive
lowering handlers also fail. Runtime handlers use the diagnostic sink and explicit
typed C++ failure status; callers propagate failure without duplicate reports, fake
terms, swallowed errors, unnecessary aborts or escaping C++ exceptions. The harness
exits nonzero on unhandled failure and tears down normally.

Unused placeholders, successful lifecycle, optional optimization skips and sound
generic fallback stay silent. Distinguish deferred features from invalid/unknown
inputs, missing SDKs, I/O errors and bugs. Remove stale catalog/capability paths only
after implementing and testing semantics; never contaminate stdout or artifacts.

Step 8's [reporting contract](../docs/features.md) records the canonical catalog,
existing/planned extension points, shared context spelling, failure propagation,
compiler delivery flags and runtime C++ status/sink behavior. Reporters are tested
in isolation; steps 14/17 place actual service/capability handlers. Existing CLI
frontend behavior and ordinary diagnostics remain unchanged.

### Artifact and command contract

```text
--emit <obj|llvm-ir|llvm-bc>  Write one artifact per Erlang module.
--artifact-dir <directory>  Override artifact root.
--target-triple <triple>    Select machine/OS/ABI; --target selects project targets.
-O0 | -O2                  Default generic O0; O2 enables bounded variants and LLVM O2.
--print-ir                 Print verified IR before LLVM optimization.
--print-optimized-ir       Print verified IR after the selected pipeline.
--print-types              Print declared/inferred types before lowering.
--no-type-specialization   Disable compiler-created variants regardless of option order.
```

Default compilation runs all implemented stages and verifies native object buffers
in memory; only `--emit` persists artifacts. Each positional invocation is one
batch; each selected project target is independent. Roots default to invocation-
relative `build/aot`, or manifest-relative `build/aot/<encoded-target-name>`.
Explicit roots are invocation-relative with separate project-target subdirectories.
Use reversible portable module-identity filenames; target format selects `.o`
(ELF/Mach-O), `.obj` (COFF), `.ll` or `.bc`. Descriptors retain the runtime dependency.

Keep `-o/--output` and TOML `output` reserved for future executables; reject explicit
`--emit` with explicit `-o`. Frontend check/print actions do not run the backend;
reject compilation switches with them or `--new-project`, preserving informational
precedence. Validate every batch and stage artifacts before publication, protecting
inputs/existing outputs on compilation failure. Replace complete files with tested
operations; do not claim multi-file atomicity if publication fails partway through.

### Compilation tracing and inspection

Extend `--verbose` through a shared backend callback. Preserve `[pp]`/`[parse]` and
prefix each compilation event with `[comp]` on stderr, including original source,
phase, module and applicable project target. Trace analysis, inference, lowering,
specialization, verification, optimization and emission only when they begin;
escape control characters and omit phases prevented by failure. Tracing changes
neither outcomes nor artifacts and stays absent from frontend/help/version output.

IR inspection uses the shared compiler and LLVM text printer, emits no files and
never emits machine code or links. `--print-ir` stops after verified lowering and
compiler specialization; `--print-optimized-ir` also runs/verifies the chosen LLVM
pipeline. Both flags print before/after snapshots per module, retaining pre-pipeline
text only when requested. Optimization level affects the optimized stage's pipeline.

Allow preprocessing, target-triple, project selection, optimization/specialization
and verbosity with IR inspection. Reject `--emit`, `--artifact-dir`, `--output`,
`--new-project` and frontend check/print combinations; preserve existing combined
`--print-pp`/`--print-ast` behavior. Single-module/stage stdout is LLVM assembly;
multiple snapshots use escaped LLVM-comment headers for target/module/source/stage
in stable target/module order. Document concatenation as separate modules and use
`--emit llvm-ir` for machine consumption; never stream bitcode to stdout. Print
only verified snapshots; later failures may leave earlier valid output but return
nonzero with stderr diagnostics.

`--print-types` stops after semantic/type analysis, showing stable module/function
and expression-location summaries with declared, inferred and unknown/widened
provenance. Allow preprocessing, project selection and verbosity; reject other
check/print actions, emission/output options, `--new-project`, target-triple,
optimization and specialization. Warnings/traces stay on stderr; no files or LLVM
code are produced. This report is not a new IR or stage-input format.

### LLVM dependency policy

Require one pinned stable globally installed LLVM C++ SDK via
`find_package(LLVM REQUIRED CONFIG)` behind private `erlang_codegen`. Step 1 records
the tested release/build configuration; use its headers and matching documentation.
Keep SDK headers out of parser/project/runtime public interfaces and apply imported
components locally without copying global `llvm-config --cxxflags` or weakening
C++23/warnings-as-errors. See [LLVM CMake integration](https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project).

Automatically search standard system/package-manager prefixes, including Homebrew,
and report version/prefix. `LLVM_DIR` may select a global installation only. Missing
or incompatible SDKs fail compiler configuration with required version and searched
locations; Clang alone is insufficient. Never download, clone, vendor, build or
install a private SDK through configuration, builds, tests or helpers. Global SDK
installation is an external prerequisite. Runtime-only builds never discover LLVM.

Check host architecture, standard-library/CRT, RTTI and exception compatibility;
retain project exception support. Host LLVM and emitted/runtime targets may differ.
Start native execution on macOS arm64; inspect Linux x86/x86-64/ARM/AArch64 and
Windows x86/x86-64 objects where SDK backends exist. Object inspection does not
establish native ABI/runtime/platform support.

## Validation and commit rule for every step

Every numbered step is a separate implementation commit. Complete its focused
tests, then run the shared gate below **before committing**. If a step grows
beyond its stated topic, split it and update this plan rather than combining
unrelated work. Do not commit a failing or partially implemented step.
The commit message must contain "[compiler] <step title>" and reading git history
helps establish last performed plan step. Refuse to begin work if git state is not clean.

1. Add or adjust behavior tests appropriate to that step, including meaningful
   failure cases; preserve useful CLI/preprocessor/parser/project behavior coverage.
   Follow the [test migration strategy](00-finished.md#testing-strategy-and-migration)
   and the [coverage ledger](../docs/test-migration.md)
   when replacing tests; retired implementation-coupled executables need not be recreated.
2. Run `make format` and verify formatting. Document each new function and class
   field's intent in one or two lines; keep functions and files simple.
3. Freshly configure with both compiler and runtime enabled, then build and test:

   ```sh
   CXXFLAGS= cmake --preset debug --fresh
   cmake --build --preset debug
   ctest --preset debug --output-on-failure
   cmake --build build/debug --target check-quality
   git diff --check
   ```

   Use automatic global SDK discovery. If selecting among global installations,
   record the installed SDK's `LLVM_DIR` in a reproducible local preset/environment
   or explicit configure argument; never fetch a private SDK to pass the gate. The empty
   `CXXFLAGS` avoids this host's known dependency-header warning override.
4. Lizard and clang-tidy must pass with the existing thresholds and checks.
   Do not increase thresholds, suppress findings, skip required tests, or weaken
   warnings-as-errors to make a step pass.
5. Review the diff, update this plan's validation ledger and the compact
   `.agents/arch.md` / `.agents/files.md` when applicable, then commit only the
   completed step. Preserve unrelated user changes. Record unavailable native
   platform runs as pending rather than successful.

Focused tests below supplement this shared gate; they never replace it. The
planning-only creation of this document does not run or claim these code gates.

## Implementation steps

### Test routing for the remaining steps (15–46)

Use the following destinations together with each step's Validate requirements.
The test migration changes coverage ownership, not the supported compiler subset
or completion status of any implementation step. Historical test counts below
remain historical evidence, not targets for the new suite.

| Steps | Required observable coverage and retirement condition |
| --- | --- |
| 15–19 | Real Erlang sources through positional and project CLI modes; verify located diagnostics, target isolation, no partial output and later-file recovery. Extend `frontend_cli`, `project_cli` and `project_workflow`; do not rebuild deleted project model/decoder/selection/callback suites. |
| 20–27 | Reuse parser type/specification fixtures and source-level semantic cases. Prefer supported public inspection output once step 39 exists; until then retain only focused lattice/widening/ownership invariants without an observable replacement. Never add a product switch solely for testing private state. |
| 28–35 | Extend the separately built `runtime_generated_link` consumer with generated registration, ABI round trips, calls and teardown as lowering becomes reachable. Keep current synthetic backend tests until equivalent real Erlang fixtures produce inspected artifacts; replacing a test driver alone is not migration. |
| 36–39 | CLI option matrices and project workflows verify real artifact paths, conflicting options, failure atomicity, verbose streams and inspection output. Inspect IR with LLVM tools; compare semantic structures rather than unstable complete LLVM text. |
| 40–42 | Link and execute real emitted modules through the actual runtime, compare decoded behavior with OTP at O0/O2, and exercise repeatability and deterministic resource caps. Replace synthetic emission/term tests only after case-level coverage review. |
| 43 | Inspect cross-target architecture, format, symbols and ABI widths separately from native execution. Keep mathematical 32/64-bit ABI boundary tests until both native widths provide equivalent evidence. |
| 44–45 | Keep diagnostic subprocess scenarios and real state-preservation/teardown checks. Retain deterministic allocation, sink, invalid-IR and otherwise unreachable rollback injections as documented exceptions. Run lifetime/stress cases under supported sanitizers; unavailable hosts remain pending. |
| 46 | Publish current CTest inventory with passing, failing and skipped counts separately, commands, native capabilities and sanitizer evidence. Update the migration ledger and feature catalog test references; do not label stage adapters as end-to-end Erlang compilation. |

Current replacements include complete CLI AST/diagnostic fixtures, preprocessed
token round trips, real include/encoding workflows and project directory/corpus
workflows. Remaining internal parser tests protect injected budgets, checked
handles, source ownership and raw stage contracts that the CLI cannot express.
The backend driver is still a stub: none of these frontend replacements proves
generated Erlang execution. No remaining implementation step is marked complete
by this migration.

Migration validation on Windows x64 (2026-09-28): full Debug and Release each
pass 74/75, with the retained raised-depth parser stack-overflow regression failing;
runtime-only ASan passes 15/15. Full Lizard passes, but the fresh full clang-tidy
gate has unresolved Windows exception-escape/Boost analyzer findings. Resolve
these before claiming a clean commit; do not disable the parser test or suppress
quality checks. Full frontend ASan is blocked by the installed LLVM SDK allocator
conflict. The ledger records exact evidence and remaining host coverage.

### 1. Pin the SDK and freeze the milestone contract

- Select a compatible globally installed stable LLVM release; record exact version,
  installation prefix, package/build source, license, host requirements and required
  tools in `docs/compile.md`. Document global installation as a prerequisite.
- Freeze the subset, command contract and provisional ABI decisions above;
  identify the native reference target and available cross backends.
- Validate: documented dependency locations/tool versions and command examples
  are internally consistent. Shared gate, then commit the contract.

### 2. Discover and link the LLVM SDK

- Add `cmake/LLVMDependencies.cmake` with automatic global SDK discovery, optional
  selection among global installations, required version checks and target-local
  SDK usage. Enforce fatal failure when unavailable and no private-copy fallback.
  Introduce the private `erlang_codegen`
  library without changing invocation behavior.
- Validate: a global SDK is discovered without hints and the smoke program links;
  selecting another global installation works; absent/incompatible SDKs and a
  private-copy-only environment fail clearly. Verify failure performs no download,
  bootstrap or automatic install. Runtime-only configuration remains independent
  of LLVM. Shared gate, then commit.

### 3. Define compilation ownership and results

- Add private request/result/diagnostic types under `compiler/src/codegen/`.
  Own context, module, diagnostics and output buffers with explicit lifetimes;
  expose no LLVM types through frontend or runtime headers.
- Validate: ownership, move/destruction, diagnostic propagation and independent
  compilation instances. Shared gate, then commit.

### 4. Construct the target machine

- Resolve a target triple, CPU baseline and data layout using the SDK. Initialize
  only configured backends and define relocation/code-model defaults. Never
  silently substitute host settings for an unavailable requested target.
  Default to the running host triple and detected CPU/features; explicit foreign
  triples use a generic CPU baseline. Configuration switches remain deferred.
- Validate: host layout, unknown triples, absent backends and differing target
  word sizes. Shared gate, then commit.

### 5. Establish LLVM IR verification

- Build a tiny synthetic module using `LLVMContext`, `Module` and `IRBuilder`.
  Set its triple/layout and integrate function/module verification with project
  diagnostics. Make verification mandatory before any emission.
- Validate: valid module accepted; deliberately malformed IR rejected without
  artifact output. Shared gate, then commit.

### 6. Emit a synthetic native object

- Add in-memory object emission using `TargetMachine` and the selected release's
  supported code-generation pass interface. Do not assume its pass-manager API
  is identical to the middle-end API. Keep this accessible through backend tests.
  Call step 5's `verify_ir` gate before producing any bytes; recheck the current
  batch on every emission attempt rather than caching verification success.
- Validate: inspect architecture, sections and a known symbol with LLVM tools;
  emission errors produce diagnostics. Shared gate, then commit.

### 7. Define the immediate-term ABI

- Derive the term-word contract from `runtime/include/base_types.hpp`, the tag
  sketch in `terms.hpp` and the `Term`/header layout now located privately in
  `runtime/src/terms/term_layout.hpp`.
  Reconcile definitions and assertions while preserving public `Term` access and
  future heap references. Record the chosen encoding in the sketch and versioned ABI.
- Add versioned ABI headers in `abi/include/erlang_aot/abi/` for term encoding,
  the forward-declared project context and generated-function signatures. Implement only the
  immediate integer encoding needed by this milestone.
- Validate: boundary/negative integer round trips, rejected overflow, target
  widths and native C++/LLVM layout agreement. Shared gate, then commit.

### 8. Define the future-feature catalog and reporting contract

- Add shared feature IDs/names in `abi/` and separate small compiler/runtime
  reporting interfaces without introducing an LLVM dependency into the runtime.
  Record actual extension points and standardize `[feature name] notimpl`, context
  and explicit failure status; format messages at the owning boundary only.
- Validate: stable feature names, context formatting, one report per failure,
  stderr routing and silence when no placeholder is invoked. Shared gate, then commit.

### 9. Establish runtime and process lifecycle

- Build upon the sketch's `ProcessContext`/`TermFactory` ownership and host-root
  lifetime contract when defining context creation and shutdown. Consult
  `runtime/include/process.hpp` for owned heap/mailbox state, pending-signal
  lifetime and exit invalidation. Reserve runtime-owned `CodeServer` and `AtomStorage`
  service bindings without implementing deferred services merely to fill accessors.
- Replace the empty runtime translation unit with explicit initialization,
  shutdown and owned process-context creation/destruction. Define typed C++ status
  reporting and the mandatory generated-program CMake link target.
- Validate: repeated lifecycle, independent contexts, cleanup after initialization
  failure and runtime-only builds without LLVM. Shared gate, then commit.

### 10. Add the runtime term-service boundary

- Use `runtime/design/terms.md`, `runtime/include/{base_types,terms}.hpp` and the
  private `runtime/src/terms/term_layout.hpp` as the starting contract and extend it
  into the runtime term library. Move implemented API
  declarations into `runtime/include/erlang_aot/runtime/` and keep heap layout
  structs private under `runtime/src/terms/`; update the sketch as choices settle.
- Add `runtime/src/terms/` services for immediate-term classification and checked
  integer encoding/decoding using the shared ABI. Reserve heap-term operations
  without inventing successful implementations for unsupported term kinds.
  Keep future atom construction routed through `atom_storage.hpp` and its
  runtime/module initialization contract; do not add a separate atom table.
- Validate: boundary values, malformed tags and agreement with generated integer
  constants; no dependency on compiler or LLVM libraries. Shared gate, then commit.

### 11. Add the builtin dispatch skeleton

- Add `runtime/src/builtins/` using `runtime/include/erlang_aot/runtime/{callable,code_server}.hpp`
  for module ownership and function/arity/argument-type registration. Implement the
  default all-Term signature needed here and the generated-code ABI service-result bridge;
  keep typed extensions exact and conversion-free when introduced. Reuse one
  registry per module, not a second BIF-specific overload table. Unimplemented BIFs
  report unavailable; the compiler's accepted source subset does not expand yet.
- Validate: known test registrations, duplicate signature keys, distinct arities,
  missing generic entries, frozen publication and failure propagation across the
  generated-code ABI. No conversion support is required. Shared gate, then commit.

### 12. Establish process memory ownership

- Extend the sketch's heap-prefix, alignment, tracing and rooted-handle contracts
  when defining process memory ownership; retain explicit layout assertions and
  derive widths from the runtime target. Use `runtime/include/process_heap.hpp`
  and `Term::copy_to` as the proposed ownership/copy/collection boundaries; reconcile
  word-based allocation/accounting with byte-based options before implementation.
  Include mailbox/cursor roots and independently owned pending signal payloads in
  the future ownership/collector contract; transit data must not borrow sender heaps.
- Carry `runtime/include/binary_heap_object.hpp` into the future shared binary
  boundary: checked immutable copies, exact tail lengths, stable word addresses,
  word counts strictly above the 64-byte process-heap threshold,
  and vector reclamation on final shared-owner destruction without a separate heap or pool.
  Process GC must destroy shared handles rather than byte-copying their representation;
  actual binary allocation and term-layout integration remain deferred.
- Add `runtime/src/memory/` lifecycle and ownership boundaries for process-local
  resources. Define where allocation failures and future root/safepoint support
  enter; do not implement a custom allocator or collector in this skeleton.
- Validate: separate process owners, teardown and failure cleanup under sanitizers;
  document that generated heap allocation remains unsupported. Shared gate,
  then commit.

### 13. Establish the scheduler service boundary

- Use `runtime/include/{process,scheduler,mailbox}.hpp` and
  `runtime/design/processes.md` for owner-worker, cooperative reduction, signal-inbox
  and receive-wait contracts. Reserve separate enqueue/handling boundaries: every
  message first becomes a signal, and only handling appends to the mailbox.
  Preserve signal order, bounded handling for waiting/suspended processes, explicit
  suspension and the cursor/arrival handshake. Do not implement receive in this step.
- Add `runtime/src/scheduler/` state owned by the runtime and explicit process
  registration/removal. Define lifecycle transitions and the future reduction,
  yield and wakeup entry boundaries without starting worker threads or claiming
  Erlang scheduling behavior.
- Validate: registration/removal, invalid transitions, isolation between runtime
  instances and ordered shutdown. Shared gate, then commit.

### 14. Place runtime service placeholders

- Add explicit not-implemented entry points at the existing term, BIF, memory,
  process and scheduler boundaries, with module-loading hooks where their ABI is
  defined. Keep them under `runtime/` and use the shared catalog/status contract.
  Do not invoke deferred services during supported lifecycle operations.
- Validate: direct tests of each reachable placeholder, known BIF identification,
  failure propagation, one stderr report, no fabricated results and resource
  cleanup. Unknown BIFs remain distinct from known deferred ones. Shared gate,
  then commit.

### 15. Index module and function declarations

- Add semantic module/function tables under `compiler/src/semantic/`; validate
  module identity, duplicate definitions, arity and exports. Define a reversible
  collision-free symbol encoding with no host-dependent hashing.
- Validate: missing/duplicate declarations, malformed exports, quoted/Unicode
  names and symbol collisions. Shared gate, then commit.

### 16. Enforce the supported language subset

- Add exhaustive AST capability checks and the metadata allowlist. Preserve
  original source/include locations in unsupported-feature diagnostics.
- Validate: positive subset fixtures and rejection of every excluded syntax
  family, including unused functions and behavior-changing attributes. Shared
  gate, then commit.

### 17. Place compiler capability and lowering placeholders

- Map deferred syntax/operations to the shared feature catalog in capability
  analysis. Add defensive unsupported-operation handlers at concrete lowering
  extension points and document future driver hooks without implementing linking.
- Validate: located `[feature name] notimpl` diagnostics for representative deferred
  families, errors even in unexported functions, nonzero exits and no published
  artifact for a failed batch. Supported cases remain silent. Shared gate,
  then commit.

### 18. Resolve parameter bindings

- Bind distinct named parameters, handle each wildcard independently, and resolve
  body variable references. Keep these semantic results outside the immutable AST.
- Validate: identity/projection functions, unbound names, wildcard reads and
  repeated-parameter patterns that this subset does not yet support. Shared gate,
  then commit.

### 19. Resolve the compilation-batch call graph

- Separate local/remote name, arity and export resolution from LLVM emission.
  Establish the acyclic dependency order needed for inference within each batch;
  detect unsupported executable recursion before analyzing function bodies.
- Validate: forward calls, missing/private callees, arity mismatches, cycles and
  isolation between project targets. Shared gate, then commit.

### 20. Model semantic Erlang types

- Add `compiler/src/semantic/types/` with exhaustive handling of the existing type
  AST, semantic type identities and a bounded abstract inference domain. Define
  union/join and conservative widening independently of LLVM representations.
- Validate: every AST type category, singletons/ranges, structural descriptions,
  top/bottom, stable identities and sound widening. Shared gate, then commit.

### 21. Resolve declared types and specifications

- Resolve aliases/parameters, exported remote types, opaque/nominal boundaries,
  specs/callbacks, overloads and constraints. Keep recursive type graphs bounded
  and retain source locations and declared-contract provenance.
- Validate: malformed/duplicate/undefined declarations, variable scope, recursive
  aliases, unavailable external metadata and opaque boundaries. Shared gate,
  then commit.

### 22. Infer local expression and function types

- Infer literals and parameter references; preserve argument/result relations for
  unannotated identity/projection functions. Keep unknown exported inputs as top
  and inferred implementation facts independent of user-supplied specifications.
- Validate: constants, missing/partial annotations, identity/projection summaries
  and analysis limits with conservative widening. Shared gate, then commit.

### 23. Propagate call types and check declared contracts

- Propagate freshly instantiated summaries through the resolved dependency order,
  including nested and cross-module calls. Warn on provable spec discrepancies
  without treating specs as runtime guards or representation proofs.
- Validate: cross-module singletons, polymorphic identity uses, incorrect specs,
  overload uncertainty and target isolation. Shared gate, then commit.
- When running with `--impldebug 23` log the inferred information to screen.

### 24. Lower function declarations and integer literals

- Create ABI-compatible declarations before bodies, then lower constant-return
  functions. Use compiler-side exact integer values to check representability
  before constructing LLVM constants; never truncate an Erlang literal. Consume
  analyzed types while preserving a uniform tagged ABI and generic body. Declared
  types alone must not create LLVM assumptions or unchecked unboxing.
- Validate: native objects for `value() -> 42`, negative/boundary values and
  rejected out-of-range literals; verify every resulting module. Shared gate,
  then commit.
- When running with `--impldebug 24` log the inferred information to screen.

### 25. Lower parameter references

- Lower parameter-array access and return the selected term unchanged. Keep
  argument order and pointer alignment explicit in the generated interface.
- Validate: identity and multi-argument projection functions, including identical
  argument values and unused wildcard parameters. Shared gate, then commit.
- When running with `--impldebug 25` log the inferred information to screen.

### 26. Lower resolved direct local calls

- Consume resolved local calls and inferred summaries, evaluate nested arguments
  in source order and generate calls using the shared ABI. Reuse the previous
  call-graph checks instead of resolving names during LLVM emission.
- Validate: forward calls, nested calls, wrong arity, missing functions and
  direct/indirect recursion diagnostics. Shared gate, then commit.
- When running with `--impldebug 26` log the inferred information to screen.

### 27. Lower resolved calls across modules

- Consume batch-resolved remote calls, export identities and inferred summaries.
  Emit consistent external declarations in separate LLVM modules; reuse the
  earlier call-graph validation and do not invoke native linking here.
- Validate: the `answer`/`client` example, private/missing callees, duplicate
  module identities and cross-module recursion. Shared gate, then commit.
- When running with `--impldebug 27` log the inferred information to screen.

### 28. Bind generated modules to the runtime

- Define versioned module/export descriptors and emit a registration entry that
  calls the runtime's module-registration ABI. Implement `runtime/src/modules/`
  validation and storage; require explicit registration before harness execution.
  Keep descriptor/registration symbols alive through standard LLVM/linker mechanisms.
- Build on `runtime/include/erlang_aot/runtime/{callable,code_server}.hpp`: transfer one
  unique registry into each loaded module and freeze it before publication. Register
  the generic signatures required by this subset; any later typed registrations
  use exact argument types and explicit Term fallback, with no conversion layer.
  Bridge generated-code ABI descriptors to host-side callable storage without exposing std::function
  or RTTI across the ABI. Preserve code-image/module-root lifetime through handles.
  Reserve runtime AtomStorage initialization for future atom bindings; supporting
  module-name metadata must not silently enable atom-valued source expressions.
- Validate: multiple generated modules, duplicate module/signature rejection,
  frozen registry ownership, failed publication cleanup, retained-handle lifetime,
  invalid ABI versions/term widths and missing runtime symbols at link time. Confirm
  generated calls pass the runtime-owned process context. Shared gate, then commit.

### 29. Plan bounded type-specialization candidates

- Add the speed-mode eligibility/benefit test, canonical profiles, deduplication
  and deterministic function/module/target budgets. O0 chooses no variants;
  O2 may choose useful variants; the explicit disable override always wins.
- Validate: no Cartesian-product enumeration, repeated equivalent call sites,
  adversarial multi-argument unions, hard caps, growth estimates, no-benefit
  functions and correct generic fallback when budgets are exhausted. Shared gate,
  then commit.

### 30. Lower guarded type-specialized variants

- Reuse lowering/LLVM utilities for eligible variants and bounded runtime dispatch.
  Select variants directly only with sufficient call-site proofs; otherwise use
  implemented guards and retain the generic fallback and public ABI.
- Validate: guard hit/miss, spec-only false assumptions, small-integer boundaries,
  mixed/unknown arguments, identical semantics and actual pre-optimization IR
  growth limits. Reject over-budget variants without rejecting the program.
  Shared gate, then commit.

### 31. Add standard LLVM optimization pipelines

- Use `PassBuilder` and the standard O0/O2 pipelines; register required analyses
  and verify IR before and after optimization. Do not write generic optimization
  passes or hard-code a bespoke pass sequence. See
  [LLVM's new pass manager](https://llvm.org/docs/NewPassManager.html).
- Validate: equivalent results at O0/O2 and preservation of public symbols and
  ABI declarations. Shared gate, then commit.

### 32. Serialize LLVM IR and bitcode

- Use LLVM's own text and bitcode writers on the verified module. Keep serialized
  formats as outputs; implementing their input readers is outside this plan.
  Expose reusable text serialization for the before/after inspection snapshots.
- Validate: `llvm-as`/`llvm-dis` or equivalent SDK round trips and structural
  checks using [FileCheck](https://llvm.org/docs/CommandGuide/FileCheck.html),
  avoiding brittle full-file snapshots. Shared gate, then commit.

### 33. Plan and publish module artifacts

- Implement artifact naming, native-path handling, input/output alias detection,
  staging, checked writes and publication under the artifact contract above.
  Reuse existing project path/identity utilities where their contracts fit.
- Validate: duplicate names, traversal-like atoms, spaces/Unicode, unwritable
  destinations, write failures and preservation of existing files on compile
  failure. Shared gate, then commit.

### 34. Add compilation command options

- Parse and validate `--emit`, `--artifact-dir`, `--target-triple`, `-O0` and
  `-O2`, plus `--no-type-specialization`, in the driver. O2 selects the compiler's
  speed policy and LLVM O2; O0 stays generic. Apply the disable override regardless
  of argument order. Document defaults and conflicts in help; preserve project
  `--target` and the reserved executable meaning of `--output`.
- Validate: operands, repetition/conflicts, informational precedence, no I/O
  for rejected options, O0/O2/override behavior in both input modes, and unchanged
  existing frontend modes. Shared gate, then commit.

### 35. Integrate positional compilation

- Replace the placeholder with an owned compilation batch: retain successfully
  parsed ASTs, resolve semantics and types across the batch, lower, specialize
  only under the speed policy, then optimize and emit.
  Keep per-file preprocessing isolated and latch failures before publication.
- Validate: single/multiple source commands, default in-memory compilation,
  all three explicit artifact kinds, source errors and no publication when any
  module fails. Shared gate, then commit.

### 36. Integrate project compilation

- Adapt project execution to collect a separate compilation batch per selected
  target. Preserve target order, source discovery, options and source diagnostic
  context; share the positional backend rather than adding a second compiler.
- Validate: multiple targets, subset selection, one source with different macro
  settings per target, cross-module calls within a target, isolated artifact
  directories and failure before publication. Shared gate, then commit.

### 37. Add compilation progress to verbose tracing

- Extend the shared backend request with a progress callback and implement the
  `[comp]` contract above for both positional and project compilation. Preserve
  existing `[pp]`/`[parse]` behavior and keep every trace on stderr.
  Include inference phases and accepted/skipped specialization profiles with
  reasons, including disabled-by-policy and budget/benefit decisions.
- Validate: opt-in behavior, phase order, filenames with spaces/Unicode, escaped
  control characters, target context, failures suppressing unstarted phases,
  no backend traces in frontend-only modes, and unchanged emitted artifacts.
  Shared gate, then commit.

### 38. Add intermediate-representation inspection actions

- Implement and document `--print-ir` and `--print-optimized-ir`, including their
  validation, backend stopping points, before/after snapshots and module headers.
  Reuse LLVM text serialization and the same compilation path for both input modes.
- Validate: each stage at O0/O2, both flags together, positional/project ordering,
  option conflicts, unavailable/invalid IR, nonzero exits on failure, no output
  files, and `[comp]` staying on stderr with `--verbose`. Parse individual snapshots
  with LLVM tools and check meaningful structures without brittle full-text matches.
  Compare O2 snapshots with and without type specialization and verify public ABI stability.
  Shared gate, then commit.

### 39. Add declared/inferred type inspection

- Implement `--print-types` using the shared semantic/type pipeline, deterministic
  summaries and declaration/inference provenance. Stop before LLVM lowering and
  display conservative unknowns instead of inventing precise specifications.
- Validate: annotated/unannotated functions, local/remote inference, recursive
  type declarations, diagnostics, option conflicts, positional/project ordering
  and absence of output files or LLVM emission. Shared gate, then commit.

### 40. Execute generated objects through a native harness

- Add a small C++ harness using the shared ABI and link emitted modules with
  `erlang_runtime` through the mandatory link target and configured Clang driver
  in CMake/CTest. Initialize the real runtime, register generated modules, create
  a process context, execute functions and shut down cleanly. Keep linking test-owned;
  do not implement a production Erlang startup routine or linker driver yet.
- Validate: separately emitted modules execute the example and identity/projection
  cases at O0/O2 with correct decoded results and matching ABI. Missing runtime
  linkage must fail, and incompatible module ABI registration must fail before
  execution. Shared gate, then commit.

### 41. Compare accepted programs against OTP

- Extend the existing native/OTP test approach with bounded, terminating programs
  from the accepted subset. Compare decoded values, argument order and accepted
  module/function behavior; retain explicit unsupported-input fixtures.
- Validate: fixed and generated cases at O0/O2, repeatability and useful mismatch
  diagnostics. Include annotated/unannotated equivalents, wrong specs and all
  specialization modes to catch unsafe type-driven code generation. Shared gate,
  then commit.

### 42. Validate specialization cost and limits

- Compare O0, LLVM O2 with specialization disabled, and speed mode with eligible
  variants. Record compiler time, variant count, IR/object size and execution
  time for supported inputs; keep noisy timings out of pass/fail unit tests.
- Validate: deterministic cap/size assertions, guard fallback correctness and
  no exponential growth for synthetic wide-union/high-arity inputs. Use the
  measurements to keep or reject benefit heuristics, not to assert every narrowed
  function must be faster. Shared gate, then commit.

### 43. Inspect cross-target objects

- Cover ELF, Mach-O and COFF outputs for supported SDK backends. Use
  [llvm-readobj](https://llvm.org/docs/CommandGuide/llvm-readobj.html) and
  [llvm-nm](https://llvm.org/docs/CommandGuide/llvm-nm.html) to inspect architecture,
  format, runtime references and exported symbols instead of writing binary parsers.
- Validate: target word-size/tag consistency, unavailable-backend errors and
  appropriate object extensions. Run native harnesses only on compatible hosts
  with their toolchains; record other native coverage as pending. Shared gate,
  then commit.

### 44. Audit future-feature placeholder coverage

- Cross-check the catalog against unsupported AST families and reserved runtime
  service boundaries. Cover the actual reporting/propagation path through both
  CLI input modes and the native runtime harness, rather than only helper strings.
- Validate: exact marker/context, behavior with and without `--verbose`, no stdout
  contamination or false success, no messages from unused placeholders or generic
  fallback, and clean failure/teardown at O0/O2. Shared gate, then commit.

### 45. Harden backend failure and resource handling

- Bound batch/module work and artifact sizes using the project's existing limit
  conventions. Exercise rejected input, LLVM errors and interrupted/failed
  artifact writes without asserting LLVM internal bugs are recoverable errors.
- Validate: relevant sanitizers, compiler-only/runtime-only configurations,
  cleanup and deterministic semantic outcomes. Shared gate, then commit.

### 46. Publish compiled-module examples and validation evidence

- Add `examples/compile/`, document exact SDK setup, commands, output locations,
  accepted subset, ABI version, runtime skeleton and mandatory runtime link recipe
  in `docs/compile.md` and README. Include examples of `[comp]` tracing and both
  intermediate-representation inspection actions, including combined inspection.
  Explain inference coverage and `--print-types`, the O0/O2 speed-policy distinction,
  specialization budgets, generic fallback and the disable override.
  Publish the deferred-feature inventory, placeholder message convention and the
  distinction between supported fallback and unimplemented semantics.
  Update the architecture/file maps to describe the implementation actually built.
- Validate: execute every documented native example, inspect all artifact kinds,
  and record platform/SDK/O0/O2 results without claiming unsupported runtime
  features. Shared gate, then commit this milestone's documentation.

## Completion criteria and later work

This plan is complete when real accepted Erlang modules produce verified LLVM
IR, bitcode and native objects; separate generated objects link into and run in
the native test harness with the real runtime; runtime lifecycle, service
boundaries and generated-module registration are implemented and tested;
semantic failures are located and prevent publication; `--verbose` reports
`[comp]` progress and inspection actions show declared/inferred types and verified
IR; inference fills missing annotations conservatively and speed-mode specialization
obeys its budgets while preserving generic behavior and runtime ABI; reached
future-feature placeholders report `[feature name] notimpl` with explicit failure
and no incorrect results or artifacts;
and every implementation step has its passing gate and separate commit.

Later plans fill out the runtime skeleton with term allocation and bignums, BIF
implementations, proper tail calls, process execution, scheduling, GC and
exceptions, alongside pattern/guard lowering and executable
startup/linking, debug information, profiling and LTO. Before allocating movable
terms, select and validate a rooting/safepoint design; reuse LLVM mechanisms
where suitable without outsourcing collector policy to them. Before suspension,
compare explicit continuations with LLVM coroutine lowering. These are bounded
design questions for those milestones, not prerequisites for constant/identity
object modules.

Do not create intermediate-stage parsers. Their only reserved locations remain
`compiler/src/stage_readers/{preprocessed,abstract,ir}/`.

## Validation ledger

Steps 1–27 each passed a fresh compiler+runtime Debug configure/build, the full
CTest suite, Lizard, clang-tidy, formatting and whitespace checks before their
individual commits. Counts below describe those historical suites; test migration
later changed the inventory. Cross-target checks do not establish native execution.
Implementation contracts and test routing remain in the sections above.

### SDK and ABI: steps 1–8 (2026-09-24, macOS arm64)

Global Homebrew LLVM 23.1.1_1 / SDK 23.1.1 was used. Native Linux/Windows execution
was pending at these checkpoints; focused ASan/UBSan passed where listed, but
LeakSanitizer was unavailable.

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

Native Linux/Windows/32-bit runtime execution and LeakSanitizer remained pending.
Runtime-only builds stayed LLVM-free; steps 10–14 also recorded Release validation. Allocation-failure injection covered
rollback and cleanup; no heap allocation, workers or generated Erlang execution
were claimed.

| Step | Delivered and validated | Full CTests | Runtime-only / focused ASan/UBSan |
| --- | --- | --- | --- |
| 9 | Runtime/context lifecycle, stable identities, token invalidation and ordered teardown; independent lifetimes, busy preservation, rollback and mandatory generated-program runtime linkage. | 84 | 10 tests; lifecycle/failure checks |
| 10 | Structural immediate-term classification and checked native integer services; malformed/pointer-shaped values, private layouts and agreement with LLVM constants. | 86 | 11 tests; 3 term tests |
| 11 | Frozen module registries, pinned generic dispatch, immediate-only Terms and status/output bridge; publication rollback and missing/unavailable BIF reporting. | 89 | 14 tests; 4 dispatch/failure tests |
| 12 | Lazy process memory ownership, checked budgets and allocation-free immediate copying; explicit unavailable allocation/collection and future root/resource contracts. | 90 | 15 tests; 5 memory/lifecycle tests |
| 13 | Scheduler registration and lifecycle bookkeeping; checked transitions, teardown order and registration rollback/retry. | 91 | 16 tests; 5 scheduler/lifecycle/memory tests |
| 14 | TermFactory, memory, send, execution and unload reporting boundaries; known-deferred versus unknown BIFs, state preservation and once-only propagation. | 93 | 18 tests; 6 service/memory/dispatch/failure tests |

After step 9, a user-directed C++ API revision removed the C headers and lifecycle
adapter. Namespaced ABI constants, scoped status and the real ProcessContext became
the sole project-internal interface, retaining the native generated-function machine
convention. The full 84-test gate, runtime-only 10 tests, focused sanitizers, C++
consumer and archive-symbol checks passed again. Earlier C compilation/link/run and
six-triple header evidence refers to the superseded API, not the current contract.
Minor destructor/test expectation findings in steps 11–14 were corrected before
the final passing gates; no checks or thresholds were weakened.

### Windows transition and reference refresh (2026-09-28)

The initial steps 14–19 attempt reproduced existing exception-escape/Boost analyzer
findings and the raised-depth parser crash; it advanced no numbered step. The OTP
reference moved to official `maint-29` at
`21776803ecd11f5fa948732c0ec66b8f325dedfc`; the grammar audit and ten-file corpus
retained their hashes/witnesses. Subsequent upstream checks through step 27 found
that revision unchanged. Historical validation above retains its original context.

A prerequisite repair restored all 75 CTests and the full quality gate: Windows
executables reserve 8 MiB stacks, movable frontend storage avoids throwing moves,
and numeric/binary-slice/CLI boundaries were clarified. Real CLI tests include
largest-finite-binary64 conversion. One analyzer crash inside Boost.Parser passed
on an unchanged complete rerun (`build/compile-steps/quality.log`).

### Semantic analysis and lowering: steps 15–27 (2026-09-28, Windows x64)

These gates used Clang/SDK 23.1.2 from the pre-existing `thirdparty/` installation
(no SDK download) and pinned clang-tidy 22.1.8 selected via `CMAKE_PROGRAM_PATH`.
Nondiagnostic launcher/analyzer failures in early runs passed on complete reruns;
from step 18, Windows analysis defaults to two concurrent jobs to bound memory.
All production commands, checks and thresholds remained enabled.

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

Step 23's obsolete debug-silence assertion was corrected before its passing full
rerun. Steps 24–27 expose inferred lowering inputs under their own debug prefixes;
the real-source backend adapter verifies/emits objects without linking generated
programs. Each of those four steps passed 80/80 tests with zero skips and full
quality; final step 27 analyzed all 154 production translation units. Reproduction
scripts and logs are under ignored `build/compile-steps/`.

Current stopping point: step 30 complete; step 31 has not started. Generated-module
registration, linked harness execution and bounded guarded specialization are implemented. CLI artifact publication, additional native hosts and
full frontend sanitizer validation remain pending. See
[compile](../docs/compile.md) and [test migration](../docs/test-migration.md) for
current scope and platform limitations.

Step 28: versioned descriptors and retained startup symbols, transactional runtime
registration, frozen ownership and linked real-source execution. Atom bindings remain
reserved. Fresh Windows x64 Debug: 82/82 CTests; quality validation recorded in docs/compile.md.

Step 29: canonical implementation profiles, exact-check benefit recognition and
deterministic function/module/target work, count and growth caps. O0/disable stay
generic; no-benefit source subset produces no variants. Fresh Windows Debug 83/83
CTests and full Lizard/clang-tidy (159 production commands) pass. One analyzer
crash on unchanged tree_attributes.cpp passed on a complete unchanged retry.

Step 30: fresh Windows x64 Debug compiler/runtime build, 84/84 CTests, full
Lizard/clang-tidy (162 production commands), formatting and whitespace pass.
The final analyzer crash in unchanged preprocessor/integer.cpp passed on a complete
unchanged quality retry. No step beyond 30 was started. Native execution remains
Windows x64 evidence; cross-width IR/objects do not claim execution on other hosts.

Step 31: fresh Windows x64 Debug build, 85/85 CTests and full Lizard/clang-tidy
pass. Standard O0/O2 native consumers retain registration, call results and ABI.
The 2026-09-29 official maint-29 fetch remains at 21776803ecd11f5fa948732c0ec66b8f325dedfc.

Step 32: fresh Windows Debug build, final 86/86 CTests and full Lizard/clang-tidy
(164 production units) pass. LLVM SDK text/bitcode round trips check public ABI
structures at O0/O2 and invalidated snapshots. FileCheck is absent from this SDK.
The initial test supplied unterminated assembly to the SDK reader; corrected.
Nested consumer tests require the Visual Studio environment, used for the final run.

Step 33: fresh Windows Debug build, 87/87 CTests and full Lizard/clang-tidy
(167 production units) pass. Filesystem tests cover native Unicode roots, reversible
traversal-like/case-distinct identities, replacement, duplicate paths, hard-link
input aliases, inaccessible parents and preserved outputs/temporary cleanup on failure.

Step 34: fresh Windows Debug build, 88/88 CTests and full Lizard/clang-tidy
(168 production units) pass. The public option matrix covers operands, repetition,
action conflicts, informational no-I/O precedence and both input modes. Backend
policy consumption and publication are wired by the following integration steps.

Step 35: fresh Windows Debug build, 89/89 CTests and full Lizard/clang-tidy
(170 production units) pass. Positional CLI coverage exercises in-memory compilation,
O0/O2 objects/text/bitcode, quoted identities/native paths, failed-batch preservation,
input aliases, syntax/semantic/target errors and existing frontend compatibility.
LLVM callback filters now suppress disabled optimization remarks without hiding warnings/errors.

Step 36: fresh Windows Debug build, final 90/90 CTests and full Lizard/clang-tidy
(172 production units) pass. Real project CLI tests cover target selection/order,
macro isolation, within-target remote calls, encoded roots, reserved TOML outputs,
cross-target call rejection and no publication when a later selected target fails.
The transitional optional backend policy was removed after the analyzer identified
an unchecked helper access; both input modes now always provide backend policy.

Step 37: fresh Windows Debug build, 91/91 CTests and full Lizard/clang-tidy
(176 production units) pass. CLI checks cover phase ordering, opt-in behavior,
source/target context, Unicode/control escaping, early failures, frontend-only modes
and identical emitted bytes. Focused planner tests observe acceptance and all bounded
rejection reasons. Existing verbose expectations now include the intended [comp] events.
