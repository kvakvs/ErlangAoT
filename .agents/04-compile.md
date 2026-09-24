# LLVM compilation integration plan

Status: steps 1–5 complete, 2026-09-24. Steps 6–46 remain pending.
Execute the numbered steps individually, each with passing validation and its own commit.

## Objective and current boundary

Compile a small Erlang/OTP 29 subset to verified LLVM IR, bitcode and native
objects. Prove execution with a Clang-linked C++ harness and the mandatory
`erlang_runtime` library. This milestone builds the runtime skeleton; full Erlang
behavior and a production executable launcher remain later work.

The preprocessor supplies expanded tokens to a parser owning a move-only
`ast::Module`. Parsing does not establish semantic validity. The driver's
`compiler/src/driver/frontend.cpp` calls a no-op `compile_module`: positional and
project compilation currently succeed without writing executables. Frontend
check/print actions and `[pp]`/`[parse]` tracing work. The runtime is a placeholder
static library, `erlang_aot_abi` an empty interface target; global LLVM SDK discovery/linkage is implemented, while lowering and the
generated-code ABI remain pending. Private compilation owners retain batch ASTs,
LLVM state and results; driver integration is still deferred. Planning found neither `llvm-config` on PATH nor the
usual Homebrew LLVM prefixes; this was not an exhaustive SDK inventory.

## Runtime API sketches to build upon

Review headers in `runtime/include/` and notes in `runtime/design/` define evolving
service/ownership proposals, not completed plan steps. CMake lists the prototype
headers for IDE navigation; only `src/runtime.cpp` is compiled. Extend these APIs
and update this inventory and affected steps together when sketches change.

| Header under `runtime/include/` | Sketch                                                                                           | Steps         |
| ------------------------------- | ------------------------------------------------------------------------------------------------ | ------------- |
| `base_types.hpp`                | Target `Word`, `ERL_WORD_BITS`, alignment                                                        | 7, 10, 12     |
| `terms.hpp`                     | `Term`, tags, `TermResult`, `AtomId`, process-bound `TermFactory`, explicit graph copies         | 7, 9, 10, 12  |
| `term_layout.hpp`               | Private slots, headers, heap layouts, Multiprecision `Bignum`, assertions; not a public/wire ABI | 7, 10, 12     |
| `atom_storage.hpp`              | Runtime-wide stable atom IDs, lookup, options/statistics, collection placeholder                 | 9, 10, 14, 28 |
| `process_heap.hpp`              | Owned heap, allocation/accounting, graph addition, safe-point collection                         | 9, 12         |
| `binary_heap_object.hpp`        | Shared immutable word vector and optional valid tail bits                                        | 12            |
| `process.hpp`                   | Identity/state/priority, reductions, cooperative code/context, owned signals/inbox               | 9, 12, 13     |
| `mailbox.hpp`                   | Selective cursor, asynchronous reads, append after signal handling                               | 12, 13        |
| `scheduler.hpp`                 | Scheduler/pool, options/snapshots/replies, bounded owner-worker signal handling                  | 9, 13, 14     |
| `callable.hpp`                  | `Callable`/`TypedCallable`, results/keys, one noncopyable registry per module                    | 11, 28        |
| `native_callable.hpp`           | `NativeCallable<Args...>` alias for `TypedCallable<Args...>`                                     | 11, 28        |
| `code_server.hpp`               | Code images, immutable loaded modules, pinned generic calls, runtime-wide server                 | 9, 11, 28     |

Supporting contracts: [terms](../runtime/design/terms.md),
[processes/schedulers](../runtime/design/processes.md),
[atoms](../runtime/design/atom_storage.md), and
[module registries](../runtime/design/code_server.md).
Reconcile tag/header assertions and heap word/byte units before implementation.
`TermTag::get_kind()` uses a constexpr three-level lookup; boxed kinds still need
header inspection. Immediate identities resolve to `local_pid`/`local_port`, empty
containers to `empty_tuple`/`empty_list`. CTest `runtime_term_tag` checks all 64 tag
combinations in `tests/runtime/term_tag.cpp`; it does not validate heap layouts.

Carry these ownership and service contracts into implementation:

- `ProcessContext` owns its heap/mailbox. `Term::copy_to` and `ProcessHeap::add`
  explicitly copy owned graphs; future GC traces host, continuation, mailbox and
  receive-candidate roots. Emitted widths come from target data layout, not host `Word`.
- `BinaryHeapObject::create` returns shared ownership of immutable `std::vector<Word>`
  storage. Counts must exceed `HEAP_BINARY_THRESHOLD_WORDS` (64 / sizeof(Word));
  otherwise return `BinaryHeapObjectError::invalid_size`. Optional tails count valid
  high bits in the last word, including byte-aligned tails; absent means full words.
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
- STL/std::function/RTTI interfaces are host-side C++, not the generated C ABI.
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
collision-free module/function/arity symbols and export/lifetime rules. Use C
calling convention with a live opaque process-context pointer, argument-array
pointer and term result; resolved identity carries arity and direct calls retain
the context. External callers supply valid terms. Prove C++ harness agreement on
each native platform; promise neither BEAM/general FFI compatibility nor future
tail-recursion support. GC, exceptions and suspension may revise this ABI.

Build `erlang_runtime` separately as C++23, initially static and LLVM-free. Shared
`cmake/BoostDependencies.cmake` provides Multiprecision to compiler/runtime and
runtime consumers. Runtime-only builds require Boost >=1.90, not Boost.Parser,
TOML or OTP; this wiring does not implement bignums. Generated service boundaries
use C linkage, opaque handles and explicit error/status transport; no C++ exceptions
or STL values cross them.

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
C ABI failure status; callers propagate failure without duplicate reports, fake
terms, swallowed errors, unnecessary aborts or escaping C++ exceptions. The harness
exits nonzero on unhandled failure and tears down normally.

Unused placeholders, successful lifecycle, optional optimization skips and sound
generic fallback stay silent. Distinguish deferred features from invalid/unknown
inputs, missing SDKs, I/O errors and bugs. Remove stale catalog/capability paths only
after implementing and testing semantics; never contaminate stdout or artifacts.

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
   failure cases; preserve all existing CLI/preprocessor/parser/project tests.
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
  sketch in `terms.hpp` and the private `Term`/header layout in `term_layout.hpp`.
  Reconcile definitions and assertions while preserving public `Term` access and
  future heap references. Record the chosen encoding in the sketch and versioned ABI.
- Add versioned ABI headers in `abi/include/erlang_aot/abi/` for term encoding,
  the opaque context and generated-function signatures. Implement only the
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
  shutdown and opaque process-context creation/destruction. Define C ABI status
  reporting and the mandatory generated-program CMake link target.
- Validate: repeated lifecycle, independent contexts, cleanup after initialization
  failure and runtime-only builds without LLVM. Shared gate, then commit.

### 10. Add the runtime term-service boundary

- Use `runtime/design/terms.md` and `runtime/include/{base_types,terms,term_layout}.hpp`
  as the starting contract and extend it into the runtime term library. Move implemented API
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

- Add `runtime/src/builtins/` using `runtime/include/{callable,native_callable,code_server}.hpp`
  for module ownership and function/arity/argument-type registration. Implement the
  default all-Term signature needed here and the C ABI service-result bridge;
  keep typed extensions exact and conversion-free when introduced. Reuse one
  registry per module, not a second BIF-specific overload table. Unimplemented BIFs
  report unavailable; the compiler's accepted source subset does not expand yet.
- Validate: known test registrations, duplicate signature keys, distinct arities,
  missing generic entries, frozen publication and failure propagation across the
  C ABI. No conversion support is required. Shared gate, then commit.

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

### 24. Lower function declarations and integer literals

- Create ABI-compatible declarations before bodies, then lower constant-return
  functions. Use compiler-side exact integer values to check representability
  before constructing LLVM constants; never truncate an Erlang literal. Consume
  analyzed types while preserving a uniform tagged ABI and generic body. Declared
  types alone must not create LLVM assumptions or unchecked unboxing.
- Validate: native objects for `value() -> 42`, negative/boundary values and
  rejected out-of-range literals; verify every resulting module. Shared gate,
  then commit.

### 25. Lower parameter references

- Lower parameter-array access and return the selected term unchanged. Keep
  argument order and pointer alignment explicit in the generated interface.
- Validate: identity and multi-argument projection functions, including identical
  argument values and unused wildcard parameters. Shared gate, then commit.

### 26. Lower resolved direct local calls

- Consume resolved local calls and inferred summaries, evaluate nested arguments
  in source order and generate calls using the shared ABI. Reuse the previous
  call-graph checks instead of resolving names during LLVM emission.
- Validate: forward calls, nested calls, wrong arity, missing functions and
  direct/indirect recursion diagnostics. Shared gate, then commit.

### 27. Lower resolved calls across modules

- Consume batch-resolved remote calls, export identities and inferred summaries.
  Emit consistent external declarations in separate LLVM modules; reuse the
  earlier call-graph validation and do not invoke native linking here.
- Validate: the `answer`/`client` example, private/missing callees, duplicate
  module identities and cross-module recursion. Shared gate, then commit.

### 28. Bind generated modules to the runtime

- Define versioned module/export descriptors and emit a registration entry that
  calls the runtime's module-registration ABI. Implement `runtime/src/modules/`
  validation and storage; require explicit registration before harness execution.
  Keep descriptor/registration symbols alive through standard LLVM/linker mechanisms.
- Build on `runtime/include/{callable,native_callable,code_server}.hpp`: transfer one
  unique registry into each loaded module and freeze it before publication. Register
  the generic signatures required by this subset; any later typed registrations
  use exact argument types and explicit Term fallback, with no conversion layer.
  Bridge C ABI descriptors to host-side callable storage without exposing std::function
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

- Step 1 (2026-09-24): pinned global Homebrew LLVM 23.1.1_1 / SDK 23.1.1;
  verified paths, tool versions, build metadata and documentation links. Frozen
  subset, CLI/artifact and provisional ABI contract in `docs/compile.md`.
  Fresh Debug compiler+runtime configure/build, all 65 CTests, make format,
  Lizard, clang-tidy and git diff --check passed. Native reference: macOS arm64;
  other native platforms and generated-code execution remain pending.

- Step 2 (2026-09-24): private `erlang_codegen` and global-only LLVM discovery/link
  probe implemented without CLI changes. Automatic and canonical-path selection,
  missing/private-only/private-path rejection, incompatible version policy and
  runtime-only configure/build passed; no download/bootstrap artifacts appeared.
  Only one distinct global SDK is installed; selection of a second installation
  and native Linux/Windows remain pending. Fresh full Debug configure/build,
  all 67 CTests, make format, Lizard, clang-tidy and git diff --check passed;
  compile commands retain C++23/-Werror with LLVM includes confined to codegen/tests.

- Step 3 (2026-09-24): private move-only request/result/compilation ownership added;
  AST provenance, empty modules and independent LLVM contexts retain explicit
  lifetimes. Diagnostic text and binary buffers survive teardown; errors invalidate
  staged batch outputs, and callback reporting failures cannot unwind into LLVM.
  Fresh full Debug configure/build, all 69 CTests, make format, Lizard, clang-tidy
  and git diff --check passed. Focused clang-tidy also passed on new tests. Backend
  and ownership/result tests passed ASan/UBSan against the existing frontend archive
  and installed SDK; LeakSanitizer is unsupported on this host and was not run.
  Opaque consumer compiled without LLVM includes. No target machine, data layout,
  lowering, verification, emission or CLI integration was added. This completed
  the original steps 1–3 request.

- Step 4 (2026-09-24): explicit target setup defaults to the running host's triple,
  CPU and detected features; foreign triples use the generic baseline. Installed
  X86/ARM/AArch64 backends initialize once; machine-derived module layouts use
  PIC/Small defaults. Unknown architectures and unavailable backends fail without
  host fallback and invalidate staged output. Native width/alignment/byte order,
  CPU/features, moves/reuse, normalized triples and Linux/Windows 32/64-bit target
  layouts passed. Fresh full Debug compiler+runtime configure/build, all 70 CTests,
  make format, Lizard, clang-tidy and git diff --check passed. Final focused target
  tests and test-source clang-tidy passed after test refinements. The same target
  suite also linked and passed against installed static LLVM components without
  libLLVM dylib linkage. Native Linux/Windows execution, object emission and CLI
  target switches remain pending. Stopped before step 5.

- Step 5 (2026-09-24): `verify_ir` checks configured triple/layout consistency,
  every defined function and whole modules with LLVM's nonfatal verifier APIs.
  Owned project diagnostics retain module/function context and SDK details;
  errors invalidate all staged output and latch failure without duplicate reports.
  Success is never cached; every future emission entry point must recheck the
  current batch. IRBuilder synthetic fixtures cover valid bodies/declarations,
  missing terminators, mismatched return types, malformed globals, target setup
  errors, mutation after successful verification, moves and result lifetimes.
  Fresh full Debug compiler+runtime configure/build, all 71 CTests, make format,
  Lizard, clang-tidy and git diff --check passed. Focused test-source clang-tidy
  and Lizard also passed. Native evidence is macOS arm64 with global LLVM 23.1.1;
  native Linux/Windows remain pending. No emission or CLI integration was added;
  stopped before step 6.
