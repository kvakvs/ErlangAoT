# LLVM compilation integration plan

Status: proposed, 2026-09-20. No implementation steps have started.
Use and build upon the existing runtime-term library sketch:
[`terms.md`](../runtime/design/terms.md) defines the design contract,
[`terms.hpp`](../runtime/design/terms.hpp) sketches the opaque C++ API, and
[`term_layout.hpp`](../runtime/design/term_layout.hpp) sketches private heap
structs and layout assertions. This is the foundation for term implementation;
refine its open decisions as the steps below are implemented. The sketch remains
outside the build with no runtime implementation, and its existence does not
complete a numbered implementation step.
Use the companion process/scheduler sketch as the runtime ownership and execution
reference: [`processes.md`](../runtime/design/processes.md) describes the contracts,
[`process.hpp`](../runtime/design/process.hpp) defines the process/context and tick
interfaces, and [`scheduler.hpp`](../runtime/design/scheduler.hpp) defines per-CPU
scheduling and process control. [`process_heap.hpp`](../runtime/design/process_heap.hpp)
adds process-owned term storage, cross-heap copying and a collection boundary;
[`mailbox.hpp`](../runtime/design/mailbox.hpp) defines selective-receive cursors
that suspend at the mailbox tail and preserve unmatched messages. These are also
review-only declarations. Use their ownership, root and suspension contracts when
implementing the service boundaries below; this milestone still defers worker
execution, messaging, heap allocation and GC rather than claiming them implemented.
This document plans the work only. Execute the numbered steps individually;
each step ends with passing validation and its own commit.

## Objective and current boundary

Produce native object modules from a deliberately small, correctly implemented
subset of Erlang/OTP 29. Also expose LLVM IR and bitcode for inspection. Prove
that generated functions execute correctly by linking objects to a small C++
test harness with Clang and the mandatory `erlang_runtime` library. Build the
runtime skeleton in this milestone; complete Erlang runtime behavior and a
production executable launcher remain later milestones.

The existing preprocessor supplies expanded tokens to the parser. The parser
owns a move-only `ast::Module`; syntax success does not establish semantic
validity. `compiler/src/driver/frontend.cpp` currently calls a no-op
`compile_module` after successful parsing. Default positional and project
requests reach that placeholder and return success without writing executables.
Explicit check/print actions and `[pp]`/`[parse]` verbose tracing already work.

The runtime is a placeholder static library; `erlang_aot_abi` is an empty
interface target. There is no existing LLVM integration or generated-code ABI.
During planning, `llvm-config` was absent from PATH and the usual Homebrew LLVM
prefixes were absent. This is not an exhaustive SDK inventory. An available
Clang executable alone does not establish availability of LLVM development
headers, libraries, CMake configuration, or inspection tools.

## Responsibility split

ErlangAoT is a **language frontend to LLVM**, not a new LLVM machine target.
Use the existing X86, ARM and AArch64 backends. Do not implement an LLVM target,
instruction descriptions, assembler, object format writer, or register allocator.

| Responsibility                                                                                         | Owner                                            |
| ------------------------------------------------------------------------------------------------------ | ------------------------------------------------ |
| Erlang source, preprocessing, syntax and source diagnostics                                            | Existing ErlangAoT frontend                      |
| Binding, module/function resolution, patterns, guards and evaluation semantics                         | ErlangAoT semantic analysis and lowering         |
| Erlang type declarations, inference and bounded type-specialization policy                             | ErlangAoT; LLVM optimizes the resulting typed IR |
| Runtime term representation and generated-function interface                                           | Shared project ABI                               |
| Mapping accepted Erlang operations to LLVM IR                                                          | Thin ErlangAoT lowering layer                    |
| Generic IR analysis, simplification, inlining and machine-independent optimization                     | LLVM standard passes                             |
| Instruction selection, legalization, scheduling, register allocation, machine-code and object emission | Existing LLVM target backends                    |
| Linking native objects and platform libraries                                                          | Existing Clang driver and platform linker/LLD    |
| Process heaps, GC policy, scheduling, reductions, mailboxes, exceptions and Erlang BIF behavior        | Erlang runtime, with compiler cooperation        |

LLVM already exposes target selection, data layout and object emission through
its SDK; use those APIs rather than building a parallel backend. See the
[LLVM object-code tutorial](https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl08.html).
Use Clang to select the appropriate linker and platform support when linking
test executables; do not construct a linker implementation or assume that
`llvm-link` links native executables. See
[Clang's toolchain description](https://clang.llvm.org/docs/Toolchain.html).

LLVM does not supply Erlang semantics. In particular:

- Arbitrary-precision integer arithmetic is not wrapping machine arithmetic.
  Future arithmetic needs proven fast paths plus correct runtime fallbacks;
  LLVM `APInt` is a compiler-side value, not an Erlang runtime bignum library.
- Atom identity, term equality/order, clause selection, guard failure and
  exception behavior must be defined before selecting LLVM instructions.
- LLVM verification checks IR consistency, not Erlang correctness. Do not attach
  `nsw`, `nuw`, `inbounds`, alias, memory-effect or exception attributes without
  satisfying their contracts. See the
  [LLVM language reference](https://llvm.org/docs/LangRef.html).
- LLVM GC support supplies compiler mechanisms such as root descriptions and
  safepoints; it does not provide this project's collector. Its example named
  `erlang` is not an OTP runtime. See
  [LLVM garbage collection support](https://llvm.org/docs/GarbageCollection.html).
- Coroutines can help lower suspension, but do not supply Erlang scheduling,
  mailbox semantics or process isolation. Tail-call optimization alone is not
  an implementation of Erlang's bounded-stack tail recursion. Defer those
  runtime/ABI decisions instead of assuming LLVM implements them automatically.

## Recommended first milestone

### Language scope

Start with ordinary named modules containing exports and single-clause
functions. Accept distinct variable or wildcard parameters and a single body
expression composed of:

- Integer literals representable by the initial tagged-small-integer ABI.
- References to named parameters.
- Direct local calls and literal `module:function(...)` calls to accepted
  functions in the same compilation batch, with nested argument expressions.

Require an acyclic call graph in this milestone. Diagnose recursive cycles,
including cross-module cycles, until proper tail calls and process stacks have
a deliberate design. Require declared exports for cross-module calls. Do not
silently resolve unknown calls to hypothetical runtime functions.

For example, these modules must eventually produce separately linkable objects:

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

Initially reject bignum literals, arithmetic, constructed heap terms, atom-valued
expressions, matching beyond variable/wildcard parameters, guards, multiple
clauses, closures, dynamic calls, exceptions, receive, concurrency and code
loading. Negative integer literal syntax needs explicit recognition; accepting
it does not enable general unary arithmetic. Reject unsupported code even in
unexported functions; optimization must not hide unsupported semantics.

File/module/export attributes and type declarations/specifications are handled
explicitly. Specify a small allowlist
of metadata that has no execution effect; reject other attributes by default.
In particular, do not ignore parse transforms, `on_load`, parameterized modules
or compile attributes that change name resolution or behavior. Syntax-only
checking retains its current broader language coverage.

This narrow slice exercises the runtime skeleton without requiring the full
runtime first. It must be labelled a subset, not advertised as full Erlang support.

### Internal representation and ABI

Use the existing AST plus small semantic tables: module identities, function
symbols, parameter bindings, resolved call sites, declared/inferred types and
capability diagnostics. Keep analysis results in side tables without rewriting
the immutable AST. Lower accepted nodes directly with LLVM `IRBuilder`. Do not
first implement a
generic SSA IR, optimizer, register model, Core Erlang/BEAM reader, MLIR dialect,
or custom LLVM pass. Add a language-specific intermediate representation later
only if a concrete Erlang transformation needs it. LLVM's
[frontend guidance](https://llvm.org/docs/Frontend/PerformanceTips.html) informs
the emitted IR; it does not require copying LLVM analyses into the frontend.

Define a private, versioned ABI in `abi/` before exposing native symbols:

- A term is an unsigned target-word-sized value. Initially only the documented
  immediate signed-integer encoding is constructed; use explicit unsigned
  encoding operations and range checks. Do not claim compatibility with BEAM's ABI.
- Use a uniform C calling convention with an opaque process-context pointer,
  an argument-array pointer, and a term result. Arity belongs to the resolved
  function identity. Even this allocation-free subset receives a live context
  created by the runtime; direct calls propagate that same context.
- Define collision-free module/function/arity symbol names, export visibility,
  ownership, pointer lifetimes and exact tag bits. External callers supply valid
  ABI terms; this is not a general-purpose FFI yet.
- Derive widths and alignment from the selected target, not host `sizeof`.
  Prove agreement with C++ harness declarations on each native test platform.
- Permit an explicit ABI revision when GC, exceptions or suspension arrive.
  Do not promise this initial stack-based call convention supports tail recursion.

Keep the runtime separately buildable and free of LLVM SDK dependencies. Runtime
service declarations use C linkage and opaque handles; C++ exceptions and STL
types must not cross the generated-code boundary. Define explicit status/error
transport for service calls before exposing them to generated functions.

### Erlang type analysis and simple inference

Type analysis is a required stage before LLVM lowering. Consume the existing
`ast/types.hpp` and type/specification forms in `ast/forms.hpp`, rather than
parsing annotation text again. Erlang types describe sets of terms; LLVM types
describe their chosen machine representation. Follow
[OTP's type/specification contract](https://www.erlang.org/doc/system/typespec.html).

The pipeline becomes declaration/binding/call resolution, declared-type resolution,
inference, generic lowering, optional bounded specialization, then LLVM optimization
and emission. Keep user-declared contracts distinct from facts proven by analysis.

- Understand `-type`, `-opaque`, `-nominal`, `-export_type`, `-spec` and `-callback`,
  including aliases, type parameters, remote references, overloads and `when`
  constraints. Preserve opaque/nominal identity and visibility across modules.
- Handle every existing type AST alternative explicitly. Represent built-in term
  categories, singletons, ranges, unions, variables and structural tuple/list/map/
  record/bitstring/function types. Understanding their annotations does not enable
  construction or operations on those values in the initial executable subset.
- Use a small inference domain: `term()` as top, `none()` as bottom, singleton and
  category facts, bounded integer ranges/unions and simple parameter/result
  relations. Unknown information is top, never bottom. Retain richer declarations
  symbolically when analysis cannot reason about them precisely.
- Resolve available exported remote types from the batch. Diagnose malformed,
  duplicate or undefined local declarations; unavailable external type metadata
  yields conservative unknown information with a diagnostic, not an invented type.
  Resolve recursive type aliases using memoized graph nodes rather than infinite
  expansion. Recursive types are distinct from unsupported executable recursion.
- Infer missing types from literals, parameters and direct calls. An unannotated
  `identity(X) -> X` must preserve the argument/result relation; instantiate it
  independently at each call. Propagate summaries through the acyclic call graph
  in dependency order, including cross-module calls, separately per project target.
- Bound expansion, union size and analysis work. Widen to safe supertypes when
  precision budgets are reached; resource exhaustion beyond the work budget is a
  clear diagnostic. Uncertain code retains the generic tagged-term path.
- Analyze exported parameters as arbitrary valid terms. A written spec is not a
  runtime guard or representation proof. Infer implementation facts independently
  and warn on provable spec contradictions; do not reject otherwise valid dynamic
  Erlang solely because its specification is narrower than its behavior.

Acceptance examples: `value() -> 42` infers a singleton integer result;
`identity(X) -> X` infers a parameter/result relation;
`answer:identity(answer:value())` infers the singleton across modules. Annotated
and unannotated equivalents must behave identically. Incorrect specs must not
introduce invalid LLVM assumptions or change execution results.

This is a small compiler analysis, not full
[Dialyzer success typing](https://www.erlang.org/doc/apps/dialyzer/dialyzer.html).
OTP tools may provide comparison evidence, but are not production dependencies
for inference. LLVM cannot recover Erlang type semantics from a generic term word.

### Bounded specialization by inferred type

Keep a generic implementation for every accepted function. Under the explicit
`-O2` speed-optimization policy, when one or a few concrete argument-type profiles
enable cheaper lowering, pre-generate specialized variants ahead of time.
Default `-O0` disables type specialization but still runs type analysis for
diagnostics and correct lowering. Type precision creates an opportunity, not a guarantee
of faster execution: dispatch, boxing and code size can outweigh the saving.

- Preserve the public tagged-term/runtime ABI. Specialized entries are internal
  implementation details. Select them directly at proven call sites; otherwise
  use a bounded sequence of safe runtime type tests and fall back to the generic
  body. A failed test must not become an Erlang error or change evaluation order.
- A profile is a canonical tuple of relevant argument facts, not the Cartesian
  product of all argument unions. Generate candidates from statically established
  call-site profiles; keep unrelated arguments generic and reuse equivalent
  candidates. Do not clone once per literal value or per calling context.
- Initial hard limits: at most three specialized variants per source function,
  32 per Erlang module and 128 per compilation target, plus the generic bodies.
  Bound candidate analysis as well as emitted variants; exceeding a budget uses
  the generic path, not a compilation failure. Deduplicate/rank candidates in a
  stable order and prevent recursive clone generation through callees.
- Also limit compiler-generated IR growth, including dispatch: no more than 2x
  the generic baseline per function and per module before LLVM optimization.
  Reject oversized candidates before publication and keep generic code. These
  are initial compiler defaults to validate, not promises about final machine-code
  size; LLVM inlining and its own transforms can affect that separately.
- Use a simple benefit test: the profile must remove an actual dynamic operation,
  tag check, conversion or dispatch cost. Pure identity/constant functions may
  need no variants. Do not generate redundant clones just to reach the limit.
- Begin with representations whose operations and guards are implemented, such
  as tagged-small-integer facts. `integer()` also includes bignums and is not
  proof of a machine-sized payload. Unsupported types and mixed unknown cases
  keep the generic path; specialization does not expand language support.
- Emit unchecked unboxing or LLVM assumptions only inside a region dominated
  by the required proof/test. Specs alone never justify them. Preserve overflow,
  exceptions, allocation, GC-rooting and process-context rules as those operations
  become supported; use correct runtime fallbacks instead of machine wrapping.

ErlangAoT owns the profiles, guards, eligibility and budgets. First use ordinary
LLVM constant propagation, inlining and simplification where they achieve the
same result. For remaining useful variants, reuse
[LLVM cloning utilities](https://llvm.org/doxygen/Cloning_8h.html) where suitable,
or the same lowering visitor with a different proven type environment; do not
write a second optimizer. Apply the normal verified LLVM pipeline to each body.

Add `--no-type-specialization` for baseline comparisons and diagnosis.
`-O2 --no-type-specialization` retains LLVM's O2 pipeline but disables compiler-created
type variants. With `-O0`, the override is accepted but has no additional effect.
Resolve this override after the optimization level, independently of option order.
Speed-mode specialization remains subject to the benefit test and budgets. Report accepted
and skipped profiles/reasons under `[comp]` verbosity. Measure compile time,
pre/post-optimization size and runtime on representative supported inputs; use
results to tune policy, without making noisy timing a required unit-test gate.

### Mandatory runtime skeleton

All runtime implementations live under repository-root `runtime/`. `abi/` owns
only the shared contract, while the compiler emits calls and descriptors against
that contract. Build `erlang_runtime` as a separate C++23 static library initially.
Every runnable link of produced modules, including test harnesses, must include
the matching runtime. There is no freestanding or optional-runtime program mode.

Native `.o`/`.obj` emission itself is not a final link: objects contain module
registration/runtime ABI references, and the runtime is linked once when those
objects are assembled into a runnable artifact. IR and bitcode retain equivalent
declarations. Do not embed a runtime copy into each module or misuse LLVM IR
linking to combine a native archive with an object file. The harness link step
must exercise this dependency, not supply test-only replacement runtime symbols.

| Location under `runtime/`                        | Responsibility and initial boundary                                                                                                 |
| ------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------- |
| `include/erlang_aot/runtime/`, `src/runtime.cpp` | Runtime startup/shutdown and host embedding API; own runtime-wide state                                                             |
| `src/process/`                                   | Opaque process contexts, ownership and lifecycle; reserve reductions, mailbox and exception state                                   |
| `src/terms/`                                     | Term inspection/manipulation services; initially immediate integers, later atoms, lists, tuples, maps, binaries and numeric helpers |
| `src/builtins/`                                  | Module/name/arity dispatch for Erlang BIF implementations; missing entries return an explicit unsupported/unavailable result        |
| `src/memory/`                                    | Per-process memory ownership and allocation boundary; reserve heap/root/GC integration without pretending a collector exists        |
| `src/scheduler/`                                 | Scheduler-owned process registration and lifecycle; reserve runnable queues, reductions, yielding and message wakeups               |
| `src/modules/`                                   | ABI-checked registration of generated module/export descriptors and explicit initialization ordering                                |

Build the runtime term library upon the existing [term sketch](../runtime/design/terms.md): a common opaque
`Term` value API, per-type creation/predicates/extraction and immutable updates.
Private word-aligned heap structs and one-word term slots keep memory layout
controlled and permit later immediate/tagged values without exposing encoding
to callers. Resolve ownership, roots, layout and the C ABI bridge by extending
this sketch, keeping its API and layout notes synchronized with implementation.
Its full API inventory does not expand this milestone's executable subset.

Implement useful skeleton behavior: create/destroy runtime and process contexts,
inspect immediate terms, report unavailable BIFs, initialize/tear down memory and
scheduler service state, and register compatible modules. Do not add functions
that return fabricated successful Erlang results for unimplemented features.
Generated heap operations, scheduling operations and BIF calls remain rejected
until their actual semantics are implemented and tested.

Use explicit initialization called by the embedding harness, not hidden global
constructors. Registration checks ABI version and term width before execution;
the harness obtains a live process context and uses it for generated calls.
One runtime instance can own several isolated process contexts. Teardown order
must release contexts and their owned resources before runtime-wide services.

Add a reusable CMake interface target for the mandatory generated-program link
dependencies. It includes `erlang_runtime` and its platform libraries, but never
the host LLVM SDK. Clang and the platform linker perform the actual link. For
cross compilation, select a separately built runtime for the emitted target;
never link the host runtime into a foreign-target program. Compiler-only builds
can still emit objects, but cannot run/link them without a supplied target runtime.

### Future-feature placeholders and diagnostics

Place explicit placeholders at real compiler/runtime extension points wherever
the surrounding interface is known. Use one shared feature identifier/catalog
and small reporting helpers, rather than scattered strings or empty functions.
A reached placeholder reports a stable message on stderr, for example:

```text
[pattern matching] notimpl: src/example.erl:12:5
[garbage collection] notimpl: process memory service
[builtin erlang:spawn/1] notimpl
```

The `[feature name] notimpl` marker is required; append available source, module,
project target or runtime operation context. These are actionable diagnostics,
independent of `--verbose`, not `[comp]` progress messages. Never put them in
printed source/type/IR output or generated artifact bytes.
Leave references to unimplemented plan filename and step or a generous TODO comment explaining what should be implemented here.

| Extension point                    | Deferred features to identify explicitly                                                                                                                           |
| ---------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Semantic analysis and lowering     | Patterns/guards, records, general arithmetic/bignums, heap-term construction, closures, dynamic calls, exceptions, receive, proper tail calls and parse transforms |
| Runtime term/BIF services          | Unsupported term operations and known unimplemented Erlang BIFs, identified by module/name/arity                                                                   |
| Runtime process/scheduler services | Spawn, message delivery, mailbox receive, yielding, reductions and scheduling                                                                                      |
| Runtime memory services            | Process-heap allocation operations, roots/safepoints and garbage collection not provided by the skeleton                                                           |
| Runtime module services            | Dynamic code loading/upgrades and unsupported initialization hooks                                                                                                 |
| Driver/toolchain integration       | Future executable startup/linking when explicitly requested through an implemented command boundary                                                                |

Use the following behavior contract:

- Known unsupported source constructs fail at capability analysis, before lowering
  or artifact publication. A defensive lowering placeholder also fails if such a
  construct reaches it unexpectedly. Do not generate a successful-looking native
  module whose unsupported operations merely print a message at execution time.
- Runtime service placeholders report through the runtime's diagnostic sink and
  return an explicit not-implemented status through the C ABI. Callers must stop
  the affected operation and propagate failure; never synthesize `ok`, zero or a
  valid-looking term. The host harness prints to stderr by default and exits
  nonzero on an unhandled placeholder failure, performing normal teardown.
- Report each failure once at its owning boundary; propagation must not print it
  again. Do not swallow failures, abort the host unnecessarily or let C++ exceptions
  cross the generated-code ABI. A runtime with several contexts retains enough
  operation/context information to attribute the failure.
- Unused placeholders remain silent. Do not call them from ordinary initialization,
  supported compilation, successful skeleton lifecycle or help/version handling.
  Skipping optional optimization or using a correct generic fallback is supported
  behavior and must not produce `notimpl` diagnostics.
- Distinguish known deferred capabilities from invalid inputs, unknown function
  names, missing LLVM SDKs, I/O errors and internal bugs; keep those existing error
  categories. Type-inference uncertainty with a sound generic fallback is not an
  unimplemented-feature failure.
- Keep an inventory mapping feature IDs to the owning file/boundary, supported
  status and a focused failure test. Define only interfaces needed at actual
  extension points; do not prebuild unused subsystems or intermediate-stage readers.
  Replacing a placeholder requires implementation and semantic tests, then updates
  to the catalog and capability checks so stale `notimpl` paths are removed.

### Artifact and command contract

Keep first artifact emission explicit to avoid reinterpreting existing future
executable destinations. Proposed options are:

```text
--emit <obj|llvm-ir|llvm-bc>  Compile and write one artifact per Erlang module.
--artifact-dir <directory>  Override the root directory for emitted modules.
--target-triple <triple>    Select machine/OS/ABI; --target still selects a project target.
-O0 | -O2                  Default O0 uses generic code; O2 optimizes for speed,
                           enabling bounded type specialization and LLVM O2.
--print-ir                 Print verified LLVM IR before optimization to stdout.
--print-optimized-ir       Print verified LLVM IR after the selected O0/O2 pipeline.
--print-types              Print declared and inferred Erlang types before lowering.
--no-type-specialization   Keep generic bodies for comparison and diagnosis.
```

- Default requests still engage every implemented compiler stage. After
  integration, generate and verify native object buffers in memory, returning
  success only when compilation succeeds. `--emit` requests persistence of
  objects or the selected IR form. Standalone executable linking stays deferred.
  This preserves the current default no-output contract while replacing its no-op.
- Default artifact roots are `build/aot` relative to the positional invocation,
  and `build/aot/<encoded-target-name>` relative to the manifest for projects.
  An explicit artifact root is relative to the invocation; project targets
  retain separate encoded subdirectories. Module filenames use a reversible,
  portable encoding of module identity, not potentially colliding source stems.
- Use `.o` for ELF/Mach-O objects, `.obj` for COFF, `.ll` for text IR and `.bc`
  for bitcode. Derive format from the selected target, not the compiler host.
- Every artifact records the runtime ABI dependency through its generated module
  descriptor/registration entry. Document the matching runtime archive and link
  recipe; object-only output is not a self-contained runnable Erlang module.
- Preserve `-o/--output` and TOML `output` as reserved executable destinations;
  neither becomes an artifact directory or an object filename. Preserve existing
  validation, reject explicit `--emit` combined with explicit `-o`, and document
  that the manifest's future executable path is not written by this milestone.
- Existing frontend check/print modes do not invoke the backend or emit artifacts.
  Reject compilation-only switches mixed with those modes or `--new-project`;
  preserve informational precedence and all existing frontend options. The new
  IR inspection actions run the backend only through the requested IR stage.
- Each positional invocation is one compilation batch; each selected project
  target is its own batch. No implicit dependencies between project targets.
- Validate all selected batches and stage their requested artifacts before
  publication. Never truncate inputs or existing outputs on compiler failure.
  Publish each complete file by a tested replacement operation; do not claim
  whole-invocation atomicity if publishing several files fails partway through.

### Compilation tracing and intermediate inspection

Extend the existing `--verbose` option; do not add a separate verbosity switch
for the backend. Keep `[pp]` and `[parse]` ingestion messages, and emit compilation
progress on stderr with `[comp]` at the start of every trace line. Include the
original source filename, the phase, module identity and project target when
applicable. For example:

```text
[comp] src/answer.erl stage=lower module=answer target=app
[comp] src/answer.erl stage=optimize module=answer target=app
[comp] src/answer.erl stage=emit-object module=answer target=app
```

Trace semantic analysis, type inference, lowering, specialization, verification,
optimization and emission only
as each operation actually begins. A failed phase must not produce messages for
later phases it prevented. Escape control characters in paths/names to keep one
event per line. Compilation traces must not appear in frontend-only actions,
help/version output, LLVM text, bitcode or native objects. Ordinary diagnostics
retain their source/context formatting; verbose tracing does not affect success
or failure. Carry a progress callback through the shared backend rather than
duplicating trace logic in positional and project drivers.

`--print-ir` and `--print-optimized-ir` are explicit inspection actions, usable
with positional inputs and selected project targets. They run the same semantic
analysis and lowering as compilation, use LLVM's existing textual printer and
produce no artifact files or executable. Do not add a custom IR representation
or printer merely for inspection.

- `--print-ir` stops after verification of lowered IR, before optimization.
  `--print-optimized-ir` also runs the selected O0/O2 pipeline and verifies its
  result. Neither action runs machine-code emission or links a runtime.
- Allow both flags together: retain the pre-optimization text before mutating
  the module and print before/after snapshots in that order for each module.
  Allocate snapshots only when requested. An explicit optimization level affects
  only the optimized snapshot, not the meaning of the unoptimized one.
- Allow target-triple, optimization, preprocessing configuration, project target
  selection and `--verbose` with inspection. Reject combinations with `--emit`,
  `--artifact-dir`, `--output`, `--new-project`, or existing frontend check/print
  actions. Existing `--print-pp`/`--print-ast` combinations remain unchanged.
- For a single module/stage, stdout is valid LLVM assembly. For multiple modules
  or both stages, add escaped LLVM-comment headers identifying target, module,
  source and stage in stable target/module order. Document that this concatenated
  inspection output is not one LLVM module; use per-module `--emit llvm-ir`
  artifacts for machine consumption. Do not stream binary bitcode to stdout.
- Print only verified snapshots. If later work fails, previously printed valid
  snapshots may remain on stdout, with diagnostics on stderr and a nonzero exit.
  Never publish invalid IR as if inspection succeeded.

The IR snapshots include the selected variant bodies and any guards before/after
LLVM optimization. `--no-type-specialization` is valid for compilation and IR
inspection, but not for frontend-only actions. The unoptimized snapshot is taken
after the compiler's specialization stage, before the LLVM optimization pipeline.

`--print-types` runs only the shared semantic/type analysis stages and prints
stable module/function and expression-location summaries to stdout. Distinguish
declared contracts, inferred facts and unknown/widened results; warnings and
`[comp]` traces stay on stderr. Permit preprocessing configuration, project target
selection and verbosity; reject other check/print actions, `--emit`, output
destinations, `--new-project`, target-triple, optimization and specialization
switches. Emit no files or LLVM code. This semantic report is not a new compiler IR
or stage-input format, and missing specifications are normal input.

### LLVM dependency policy

Require a globally installed LLVM C++ SDK through `find_package(LLVM REQUIRED CONFIG)`, behind a
private `erlang_codegen` target. Keep LLVM headers out of parser, project model
and runtime public interfaces. Support one pinned stable LLVM major initially;
record the exact tested release and SDK build configuration in step 1. The
unversioned online documentation can describe development APIs, so implementation
must follow the chosen release's installed headers and matching documentation.

Automatically search standard system and installed package-manager prefixes,
including Homebrew on macOS, on compiler-enabled configuration. A user should
not need to supply a private dependency path for a normal global installation.
An explicit `LLVM_DIR` may select an existing global installation, but must not
enable a repository-local, build-tree, vendored or downloaded LLVM SDK fallback.
Report the selected version and installation prefix.

If no compatible globally installed SDK is found, stop configuration with a
fatal, actionable error describing the required version and searched locations.
Do not silently disable compilation or continue with only a Clang executable.
Never download, clone, vendor or build a private copy of LLVM: no `FetchContent`,
`ExternalProject`, dependency bootstrap, automatic package installation, or retry
that obtains an SDK. This rule applies to configuration, builds, tests and helper
scripts. Installing the global SDK is an external prerequisite, not a build step.

Import supported SDK targets/components with target-local usage requirements;
do not copy `llvm-config --cxxflags` globally or change C++23/warnings-as-errors. LLVM's
[CMake integration documentation](https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project)
describes the SDK entry point.

Validate host architecture, standard-library/CRT, RTTI and exception compatibility.
LLVM is a host dependency; emitted target code and the future runtime may have
a different target. Keep exception support needed by existing project code.
Runtime-only builds must configure without finding LLVM. A compiler build with
the new backend requires a compatible SDK and gives an actionable error otherwise.

Native macOS arm64 is the first execution baseline. Then inspect object output
for Linux x86/x86-64, ARM/AArch64 and Windows x86/x86-64 as enabled by the SDK.
Cross-object emission is not evidence of native ABI/runtime/platform support.

## Validation and commit rule for every step

Every numbered step is a separate implementation commit. Complete its focused
tests, then run the shared gate below **before committing**. If a step grows
beyond its stated topic, split it and update this plan rather than combining
unrelated work. Do not commit a failing or partially implemented step.

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
- Validate: inspect architecture, sections and a known symbol with LLVM tools;
  emission errors produce diagnostics. Shared gate, then commit.

### 7. Define the immediate-term ABI

- Derive the term-word contract from the sketch's private `TermSlot` boundary,
  refining tag encoding while preserving opaque public `Term` access and future
  heap references. Record the chosen encoding in the sketch and versioned ABI.
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
  `runtime/design/process.hpp` for owned heap/mailbox state and exit invalidation.
- Replace the empty runtime translation unit with explicit initialization,
  shutdown and opaque process-context creation/destruction. Define C ABI status
  reporting and the mandatory generated-program CMake link target.
- Validate: repeated lifecycle, independent contexts, cleanup after initialization
  failure and runtime-only builds without LLVM. Shared gate, then commit.

### 10. Add the runtime term-service boundary

- Use `runtime/design/{terms.md,terms.hpp,term_layout.hpp}` as the starting
  contract and extend it into the runtime term library. Move implemented API
  declarations into `runtime/include/erlang_aot/runtime/` and keep heap layout
  structs private under `runtime/src/terms/`; update the sketch as choices settle.
- Add `runtime/src/terms/` services for immediate-term classification and checked
  integer encoding/decoding using the shared ABI. Reserve heap-term operations
  without inventing successful implementations for unsupported term kinds.
- Validate: boundary values, malformed tags and agreement with generated integer
  constants; no dependency on compiler or LLVM libraries. Shared gate, then commit.

### 11. Add the builtin dispatch skeleton

- Add `runtime/src/builtins/` with explicit registration/lookup by module, name
  and arity and a uniform service-result contract. Unimplemented BIFs report
  unavailable; the compiler's accepted source subset does not expand yet.
- Validate: known test registrations, duplicate entries, unknown names/arities
  and failure propagation across the C ABI. Shared gate, then commit.

### 12. Establish process memory ownership

- Extend the sketch's heap-prefix, alignment, tracing and rooted-handle contracts
  when defining process memory ownership; retain explicit layout assertions and
  derive widths from the runtime target. Use `runtime/design/process_heap.hpp`
  and `Term::copy_to` as the proposed ownership/copy/collection boundaries;
  include mailbox/cursor roots in the future collector contract.
- Add `runtime/src/memory/` lifecycle and ownership boundaries for process-local
  resources. Define where allocation failures and future root/safepoint support
  enter; do not implement a custom allocator or collector in this skeleton.
- Validate: separate process owners, teardown and failure cleanup under sanitizers;
  document that generated heap allocation remains unsupported. Shared gate,
  then commit.

### 13. Establish the scheduler service boundary

- Use `runtime/design/{process,scheduler,mailbox}.hpp` and `processes.md` for the
  proposed owner-worker, cooperative ticks, send and receive-wait contracts.
  Reserve the cursor/arrival handshake without implementing receive in this step.
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
- Validate: multiple generated modules, duplicate registration policy, invalid
  ABI versions/term widths and missing runtime symbols at link time. Confirm
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

Validation ledger: none yet; this is the implementation plan, not completion evidence.
