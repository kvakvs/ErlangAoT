# LLVM compilation contract

Status: contract frozen 2026-09-24; SDK integration, compilation ownership,
target setup, IR verification, in-memory object emission and the immediate-term ABI
implemented in steps 2–7. Step 8 adds the shared
[deferred-feature catalog and reporting contract](features.md), with separate compiler/runtime
reporters and typed C++ status results. Steps 14 and 17 integrate compiler/runtime
placeholders at concrete service and lowering boundaries.
Step 9 implements [runtime/context lifecycle](runtime-lifecycle.md) and the mandatory
`ErlangAoT::generated_program` CMake link target without LLVM dependencies.
Step 10 adds [runtime immediate-term services](runtime-terms.md): checked tag
classification and native small-integer encoding/decoding, independently of LLVM.
Step 11 adds [builtin dispatch](runtime-builtins.md): frozen generic module registries,
pinned native targets and an explicit status/word service bridge. No production BIF
or compiler BIF lowering is enabled.

Step 12 adds [process memory ownership](runtime-memory.md), checked unsupported
allocation/collection and immediate copying between owners. Heap values remain deferred.

Step 13 adds [scheduler lifecycle bookkeeping](runtime-scheduler.md): explicit
registration, checked transitions and ordered teardown without worker execution.

Steps 24–27 implement private generic lowering of integer returns, parameter references
and resolved local/remote calls. Step 28 adds explicit [generated-module registration](runtime-modules.md)
and separately linked native harness execution. Steps 29-30 add bounded
[specialization planning and guarded lowering](specialization.md); the current
guard-free source subset correctly remains generic. Steps 31–39 implement standard
LLVM optimization, text/bitcode serialization, artifact publication and the shared
positional/project driver. Default compilation validates
[declarations, bindings, calls and types](semantic.md), infers implementation facts,
then lowers, optimizes, verifies and emits native objects in memory. Explicit `--emit`
publishes artifacts; IR and type inspection stop at their selected boundaries.
No production executable launcher or linker driver is implemented.

The private backend's `verify_ir` gate checks target consistency, defined function
bodies and whole modules using LLVM's nonfatal verifier APIs. The object emission
entry point calls this gate on the current batch before producing bytes; success
is not cached across mutations. Failures become owned project diagnostics and discard
all staged outputs. Synthetic IRBuilder fixtures cover valid and malformed IR;
verification alone does not establish Erlang semantics or complete compilation.

`emit_objects` uses the SDK's legacy machine-code pass manager and
`TargetMachine::addPassesToEmitFile`, separately from the standard PassBuilder O0/O2 pipeline.
It emits clones to preserve original IR, replaces previous buffers on repeat calls,
and discards the entire batch on verification or emission errors. No files are
published and the batch remains incomplete until its caller completes the pipeline.
Configured backends register their assembly printers/parsers; recoverable LLVM
assembler errors flow through the owned diagnostic callback. LLVM fatal errors are
not converted into ordinary diagnostics by this in-process API.
Synthetic tests inspect Mach-O/ELF/COFF architecture, executable sections and an
`answer` symbol; they do not yet use the Erlang ABI or execute generated programs.

## SDK prerequisite

Support stable LLVM **23.1.x**, minimum **23.1.1**, initially. The reference SDK is
Homebrew `llvm` **23.1.1_1** (alias `llvm@23`), a `homebrew/core` stable arm64 bottle:

| Property | Validated installation |
|---|---|
| Global prefix | `/opt/homebrew/opt/llvm` |
| Resolved prefix | `/opt/homebrew/Cellar/llvm/23.1.1_1` |
| CMake package | `/opt/homebrew/opt/llvm/lib/cmake/llvm/LLVMConfig.cmake` |
| Headers / libraries / tools | `include/`, `lib/`, `bin/` beneath that prefix |
| SDK build | Release, assertions OFF, RTTI ON, exceptions OFF, libc++ |
| SDK linkage | Shared `libLLVM.23.1.dylib`; imported component libraries also available |
| Host / reference triple | macOS 26.6.2 arm64 / `arm64-apple-darwin25.6.0` |
| Project compiler | AppleClang 21.0.0 (`clang-2100.1.1.101`), C++23, system libc++ |
| SDK Clang | Homebrew Clang 23.1.1 |
| Package provenance | `INSTALL_RECEIPT.json` under the resolved prefix; built on macOS 26.6 with Xcode 27.0 |
| License | Apache-2.0 WITH LLVM-exception; packaged `LICENSE.TXT` |

LLVM is a host dependency: its architecture, C++ standard library and Windows CRT
must match the compiler tool, independently of the emitted target. Keep exceptions
and RTTI enabled in project code; no project exception may unwind through LLVM.
Use imported SDK components and target-local system includes/definitions, not global
`llvm-config --cxxflags`. A compile/link smoke check must establish compatibility.
The runtime remains a separate LLVM-free C++23 library.

Configuration prefers an installed SDK from standard system/package-manager prefixes
(including Homebrew). If none is found, CMake downloads the official **23.1.2** SDK
into `thirdparty/`, verifies its pinned SHA-256 checksum, and retains the archive
and extracted installation for reuse across build directories and offline reloads.
`LLVM_DIR` explicitly selects any existing installed SDK; invalid selections fail
without falling back. LLVM build trees are still rejected. Set
`ERLANG_AOT_DOWNLOAD_LLVM=OFF` to require an installed SDK without downloads.

Pinned binary archives cover Windows x64/ARM64, Linux x64/ARM64, and macOS ARM64.
They are selected for the compiler executable's architecture, independently of
Erlang output targets. Other hosts and cross-builds need a matching explicit SDK;
LLVM is not built from source automatically. System link dependencies and native
SDKs must still be installed. Runtime-only builds never discover or download LLVM.

Inspect this reference installation without changing it:

```sh
/opt/homebrew/opt/llvm/bin/llvm-config --version --prefix --cmakedir
/opt/homebrew/opt/llvm/bin/llvm-config --host-target --targets-built
/opt/homebrew/opt/llvm/bin/llvm-config --build-mode --assertion-mode --has-rtti --shared-mode
/opt/homebrew/opt/llvm/bin/clang --version
```

Required milestone tools from the same SDK are `llvm-config`, `clang`/`clang++`,
`llvm-as`, `llvm-dis`, `llvm-readobj`, `llvm-nm`, `FileCheck`, `opt` and `llc`.
The reference installation reports 23.1.1 for these tools. CMake >=3.28, a C++23
compiler, a native linker/platform SDK, Boost >=1.90, toml++ and the existing OTP/quality
tools remain project prerequisites; see [README](../README.md). Installed headers and
CMake configuration are authoritative for this release. References:
[LLVM CMake integration](https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project),
[LLVM license](https://llvm.org/LICENSE.txt).

The SDK includes X86, ARM and AArch64 for the project's intended targets. Native
execution starts with macOS arm64/Mach-O; later object inspection covers Linux
x86/x86-64/ARM/AArch64 ELF and Windows x86/x86-64 COFF. Available backends do not
establish native runtime support. Other native platforms remain pending.

## Frozen executable subset

Accept ordinary named modules with exports and single-clause functions. Parameters
are distinct variables or independent wildcards. Each body is one expression made
from small signed integer literals, named parameter references and direct local or
literal remote calls within the same compilation batch, with nested arguments.
Calls evaluate arguments in source order. Remote calls require exported functions;
the entire call graph must be acyclic. Reject unsupported code even if unexported.

```erlang
-module(answer).
-export([value/0, identity/1]).
value() -> 42.
identity(X) -> X.
```

A second module may call `answer:identity(answer:value())`; it must compile in the
same batch to a separate object. Negative literal syntax does not enable unary
arithmetic. Exclude bignums, arithmetic, atom expressions, heap-term construction,
other patterns, guards, multiple clauses, closures, dynamic calls, recursion,
exceptions, receive, concurrency and code loading. Handle file/module/export and
all existing type/spec AST forms explicitly. The initial inert metadata allowlist
is `author`, `vsn`, `doc` and `moduledoc`; reject other attributes, including
`compile`, `on_load`, parse transforms and parameterized modules. Syntax-only
checking retains its existing broader coverage.

Keep semantic/type results beside the immutable AST. Infer literals, parameter/result
relations and direct calls conservatively; exported inputs are arbitrary valid terms.
Specs are contracts, never runtime guards or unboxing proofs. Warn on provable spec
contradictions without changing dynamic semantics. O0 uses generic tagged bodies.
O2 may add proven/guarded variants with generic fallback: at most 3/function,
32/module and 128/target, with pre-LLVM IR growth at most 2x per function/module,
including dispatch. Generate no Cartesian products or clones without a benefit.

## Private generated-code ABI v1

[v1.hpp](../abi/include/erlang_aot/abi/v1.hpp) defines the versioned C++ term/context/function
types; [term.hpp](../abi/include/erlang_aot/abi/term.hpp) implements checked immediate
integer encoding for explicit 32/64-bit targets. Runtime lifecycle is implemented;
term services remain later work. This contract does not match BEAM. Native `Term` and private
headers have compile-checked one-word layouts; heap prefixes remain reservations.
`codegen::term_type` and `generated_function_type` derive LLVM types from the configured
target, rejecting unsupported widths/alignment. LLVM `CallingConv::C` denotes the
native free-function machine convention used here; it does not require C headers
or C linkage. All APIs are C++23 and private to this project. External C compatibility
can be added later if needed.

- A term is an unsigned target-pointer-width integer (32 or 64 bits), aligned to
  the target word. Immediate small integers have low four bits `0xf`, matching the
  sketch's primary/secondary small-integer tags. Payload width is `word_bits - 4`;
  the signed range is `[-2^(word_bits-5), 2^(word_bits-5)-1]`. Range-check exact
  source values before encoding with unsigned shift/OR; decode sign explicitly.
- Generated native-convention entries conceptually have signature
  `Term function(ProcessContext*, const Term* arguments)`. Arity is part of the
  resolved identity. Arguments are a borrowed, word-aligned array in source order,
  valid for the call; zero-arity calls may pass null. The context is live and
  runtime-owned, propagated unchanged through direct calls. Callers supply valid
  ABI terms. Returned terms follow context ownership; this subset allocates nothing.
- Symbol names use `eaot_v1_m<hex-module-UTF8>_f<hex-function-UTF8>_a<decimal-arity>`:
  lowercase byte hex, no normalization, canonical decimal without leading zeroes.
  Separate `eaot_v1_register_m<hex-module-UTF8>` names reserve registration entries.
  Exported entries/registration are externally visible; other functions are internal.
  These names specify project-owned LLVM symbols before platform decoration; future
  project registration binds their addresses to the C++ generated-function type.
- Descriptors declare ABI version and term width; runtime registration validates them
  before invocation. Runtime services use ordinary C++ APIs, scoped status enums,
  `std::expected` and RAII ownership. Generated entries retain simple word/pointer
  signatures; no STL values, RTTI protocol or C++ exceptions cross those entries.
- Every runnable link includes the matching target runtime once. The harness explicitly
  initializes runtime state, registers modules, obtains a context and tears down
  contexts before global services. Cross-target programs need a target-built runtime.
  GC, exceptions and suspension may revise this ABI; stack-based calls do not promise
  bounded-stack tail recursion. There is no general FFI or production launcher yet.

## Frozen command and artifact contract

The following compilation and inspection switches are implemented:

| Option | Milestone behavior |
|---|---|
| `--emit obj\|llvm-ir\|llvm-bc` | Persist one selected artifact per module |
| `--artifact-dir DIR` | Override the artifact root |
| `--target-triple TRIPLE` | Select machine/OS/ABI; `--target` retains project selection |
| `-O0` / `-O2` | Default generic O0 / bounded specialization plus LLVM O2 |
| `--no-type-specialization` | Disable variants independently of option order |
| `--print-ir` / `--print-optimized-ir` | Verified text snapshots before/after LLVM optimization |
| `--print-types` | Declared/inferred/unknown type summaries before LLVM lowering |

Default compilation verifies object buffers in memory without output files.
Positional inputs form one batch; project targets form separate batches. Default
artifact roots are invocation-relative `build/aot` or manifest-relative
`build/aot/<encoded-target>`. Explicit roots are invocation-relative, with distinct
project-target subdirectories. Encode module/target names as lowercase UTF-8 byte hex.
Use target-selected `.o` (ELF/Mach-O), `.obj` (COFF), `.ll` or `.bc` suffixes.
Validate and stage all requested batches before publishing complete files; preserve
inputs/outputs on compile failure without promising multi-file publication atomicity.

`-o/--output` and TOML `output` remain reserved executable destinations. Reject
explicit `--emit` with explicit `-o`. Frontend-only modes and `--new-project` reject
compilation switches; informational precedence stays unchanged. IR inspection allows
both snapshots together, target/optimization/preprocessing/project/verbosity options,
and rejects emission/output/frontend/new-project actions. It emits no files or
machine code. Single snapshots are valid LLVM assembly; concatenated snapshots have
escaped LLVM-comment headers and are separate modules, not one parseable module.
Print only verified snapshots and return failure if later work fails.

Type inspection permits preprocessing/project/verbosity only and rejects other
actions, output destinations, target-triple and optimization/specialization options.
`--verbose` keeps `[pp]`/`[parse]` and adds `[comp]` events on stderr only as phases
begin, including source/module/target and specialization decisions. Future-feature
failures report `[feature name] notimpl` once with context on stderr, independently
of verbosity, and propagate explicit failure without fake results or artifacts.

Implementation debugging is separate from ordinary tracing. `--impldebug <n[,n...]>`
option selects signed 32-bit decimal step IDs (repeatable, deduplicated, no spaces).
`ImplementationDebug::enabled(step)` is available in frontend/backend requests;
step-specific diagnostics use stderr and this selection instead of ordinary
verbosity. Step 23 reports inferred function inputs/results and parameter relations
with an `[impldebug 23]` prefix. Debugging changes neither inferred facts nor warning
policy and never runs inference in frontend-only check/print modes.

The full inspection/conflict, ownership, placeholder and validation contracts remain
in [the plan](../.agents/04-compile.md). Each numbered step requires its own full gate
and commit. Native generated-code execution is validated on Windows x64; additional
native host platforms remain pending. Cross-target object checks do not prove execution.

## SDK integration validation (step 2)

Compiler-enabled configuration now requires the SDK and creates private
`erlang_codegen`/`erlang_llvm_sdk` targets; no new CLI actions are enabled.
LLVM headers/definitions do not propagate to frontend or runtime compilation.
C is enabled for LLVM package dependency probes; project implementations stay C++23.
A configure-time C++23 link probe checks LLVM context/module ABI compatibility,
with RTTI and exceptions retained in project code. The `codegen_dependency` consumer also executes
that boundary in CTest.

Automatic discovery searches `/usr`, `/usr/local`, `/opt/homebrew`, `/opt/local`,
`/opt/llvm`, `/home/linuxbrew/.linuxbrew` and `/Library/Developer/Toolchains` on Unix,
and LLVM under Program Files on Windows. Versioned distro and Homebrew layouts
are included. The pinned `thirdparty/` SDK is the fallback when that search fails.
Canonical paths reject CMake build trees.
`LLVM_DIR` explicitly selects an installed package; invalid selections
fail without falling back. Compiler configuration reports searched locations,
selected version/prefix, host triple and available backends. Runtime-only
configuration does not load the dependency module.

```sh
CXXFLAGS= cmake --preset debug --fresh
cmake --build --preset debug
ctest --test-dir build/debug -R '^codegen_' --output-on-failure
# Optional selection of the existing global reference installation:
CXXFLAGS= cmake --preset debug --fresh -DLLVM_DIR=/opt/homebrew/opt/llvm/lib/cmake/llvm
```

Dependency tests use fresh configurations for automatic/explicit selection,
missing SDKs with downloads disabled, ignored private prefixes, rejected build
trees, incompatible release metadata and runtime-only builds. Automatic selection
reuses the SDK already acquired during the parent configuration.
Only one distinct LLVM installation is available on the reference host; explicit
selection is tested through its canonical Cellar path. A second independent global
installation and native Windows/Linux SDK compatibility remain unvalidated.

## Compilation ownership (step 3)

Private types under `compiler/src/codegen/` own one ordered compilation batch.
`CompilationRequest` transfers input ASTs, native source paths and options into a
move-only `Compilation`. Each instance has an independent LLVM context and one
empty IR module per input; module identifiers initially contain source paths,
not resolved Erlang module identities. Moves retain stable context/module/callback
addresses. Modules are destroyed before their context, and diagnostic storage
outlives both. Consuming the owner transfers results after LLVM teardown.

`CompilationResult` owns diagnostic text/optional logical locations and binary
output buffers, independent of the request, AST and LLVM. It starts incomplete;
ownership operations do not claim successful compilation. Pipeline callers may
stage output and explicitly mark completion. An error latches failure, discards
all batch outputs and prevents later completion/output staging. LLVM notes/remarks,
warnings and errors are copied through a context-local callback without printing;
callback formatting/allocation failure sets an observable failure flag without
unwinding through LLVM. LLVM fatal errors are not made recoverable by this handler.

The `codegen_results` consumer compiles without LLVM include paths. The internal
`codegen_ownership` tests cover retained AST/source data, multiple modules,
move construction/assignment and reuse, independent contexts, real SDK diagnostic
callbacks, failure propagation and result lifetime after compiler destruction.
Focused ASan/UBSan runs instrument the backend and these tests, using the existing
frontend archive and installed LLVM. Leak detection is unavailable on this macOS
sanitizer runtime and is not claimed.

Ownership construction does not select a target or lower syntax. Target setup is
an explicit next phase; the CLI still stops after semantic analysis.

## Target machine (step 4)

The private `configure_target` phase constructs one LLVM target machine per batch
and applies its normalized triple and data layout to every owned module. Empty
target requests use LLVM's running-process triple and detected host CPU/features.
An explicit matching native triple uses the same CPU policy. Foreign triples use
the generic CPU baseline with no host feature overrides. Host features are sorted
for stable configuration strings; no CPU/feature switches are exposed yet.

CMake selects the installed SDK's intersection with X86, ARM and AArch64. Only
these backends' target information, code generation and MC layers initialize,
once across compilation instances. Shared LLVM and component-library linkage use
the same selection. An unknown architecture or unavailable backend produces one
owned error diagnostic including the triple, invalidates staged output and leaves
modules unconfigured. Target lookup never falls back to the host.

Relocation defaults to PIC to accommodate future shared modules; the code model
is Small. Machine optimization follows the request: O0 maps to LLVM None and O2
to Default. Other target options retain SDK defaults. The target data layout,
including pointer width, comes exclusively from the machine. Configuring a target
keeps the batch incomplete, emits no artifacts and reuses the machine on repeated
calls. Moves preserve it; teardown releases modules before the machine and context.

`codegen_target` checks native pointer size/CPU/features, per-module propagation,
machine moves/reuse, normalized triples, unknown architectures and an unconfigured
RISC-V backend. Available cross backends are checked for Linux x86/x86-64/ARM/AArch64
ELF and Windows x86/x86-64 COFF layouts, including 32-bit widths on the 64-bit host.
These are target-construction tests, not object-emission or native-platform ABI
validation. Later implemented phases add verification, lowering, object emission
and explicit CLI artifact publication.

## Source lowering (step 24)

The private `codegen::lower` phase consumes batch-owned syntax and semantic/type
side tables. It creates generic native-convention declarations before constant
bodies and verifies the complete LLVM batch. Literals are checked against the
configured target width before encoding; specifications add no LLVM assumptions.
Exported entries have external linkage; private entries have internal linkage.

`codegen_lowering` parses real Erlang fixtures, runs analysis, inspects tagged
returns and emits native objects. It checks 32/64-bit endpoints and rejects host-valid
literals that overflow a 32-bit target. This is a stage adapter, not CLI artifact
publication or generated-program execution. `--impldebug 24` prints the analyzed
input facts on stderr. Normal CLI compilation still ends after analysis.

### Step 25

Parameter lowering borrows the binding table and emits a target-word-aligned
load from the original argument position. Grouped identity and three-argument
projections with unused wildcards retain tagged terms unchanged, without type
assumptions or inbounds promises. Native and 32-bit object/IR checks pass.
Debug25 exposes inferred input/result relations.

Validation on Windows x64: fresh Debug compiler/runtime build, 80/80 CTests and
full Lizard/clang-tidy pass. CLI artifact publication and native execution remain
later work. Cross-target object checks do not claim native execution on those hosts.

### Step 26

Direct local calls consume resolved identities and inferred summaries. An
iterative postorder walk evaluates nested arguments in source order, builds
aligned argument arrays and forwards the original process context. Zero-arity
calls pass an unused null argument pointer. Forward/private calls, nested calls
and identical argument positions are covered; CLI wrong-arity/missing/cycle
regressions remain active. Debug26 prints inferred lowering inputs.

Validation on Windows x64: fresh Debug compiler/runtime build, 80/80 CTests and
full Lizard/clang-tidy pass. CLI artifact publication and native execution remain
later work. Cross-target object checks do not claim native execution on those hosts.

### Step 27

Batch-resolved remote calls import exported generic declarations into separate
LLVM modules. Matching definition/import symbols are checked in emitted objects
for the answer/client example, including reversed source order; no native
linking occurs. Private/missing callees, duplicate modules and cross-module
recursion remain diagnosed by the existing semantic phase. Debug27 prints
inferred inputs. Generated-module runtime registration begins at step 28.

Validation on Windows x64: fresh Debug compiler/runtime build, 80/80 CTests and
full Lizard/clang-tidy pass. CLI artifact publication and native execution remain
later work. Cross-target object checks do not claim native execution on those hosts.

### Step 28

Generated modules now carry immutable ABI/word-width descriptors, export tables
and explicit registration entries. Runtime publication validates the descriptor,
freezes one unique generic registry and retains executable image ownership through
resolved handles. See [module registration](runtime-modules.md) for symbol and
lifetime contracts. A separate Clang consumer executes the real answer/client
objects with the mandatory runtime; linking those objects without it must fail.
Atom initialization remains reserved; this step adds no atom-valued expressions.

Validation: fresh Windows x64 Debug compiler/runtime build; 82/82 CTests,
Lizard and clang-tidy pass (156 production commands). Additional native hosts
and full frontend sanitizer validation remain pending.

### Step 29

The private backend plans bounded speed-mode variants from proven implementation
profiles, with deterministic deduplication, hard function/module/target caps and
dispatch-inclusive growth estimates. O0 and the explicit disable override retain
generic code. The current source subset has no removable representation checks,
so constants/identity/direct calls receive no variants. See [specialization](specialization.md).

Validation: fresh Windows x64 Debug compiler/runtime build; 83/83 CTests and
full Lizard/clang-tidy pass (159 production commands). A clang-tidy crash in
unchanged tree_attributes.cpp passed on an unchanged complete quality retry.

### Step 30

Private guarded lowering uses LLVM cloning/simplification utilities and retains
the generic tagged ABI. Dispatch tests only implemented small-integer tags and
forwards context and arguments unchanged. Actual clone-plus-dispatch IR growth
is checked transactionally against function/module budgets; excess variants
are discarded without rejecting the program. See [specialization](specialization.md).
Native LLVM fixtures cover guarded equivalence and executed generic fallback
after growth rejection. The accepted Erlang source subset still has no profitable
representation checks, so speed-mode source compilation correctly remains generic.

Validation: fresh Windows x64 Debug compiler/runtime build; 84/84 CTests,
Lizard, full clang-tidy (162 production commands), formatting and whitespace pass.
A final analyzer crash in unchanged preprocessor/integer.cpp passed on a complete
unchanged quality retry. At that checkpoint work stopped after step 30; the
following records describe the subsequent optimization and driver/artifact work.

Step 31 adds target-aware LLVM PassBuilder O0/O2 pipelines, with verification
before and after optimization and local analysis-manager lifetimes. Public entries
and runtime registration remain externally retained. The separate native consumer
runs the answer/client and identity checks at both optimization levels.

Step 32 uses LLVM assembly and bitcode writers on freshly verified modules.
Text snapshots own their bytes without altering staged artifacts; failed verification
discards the batch. SDK assembly/bitcode readers and structural ABI checks validate
round trips at O0/O2. FileCheck is absent from this installed Windows SDK.

Step 33 plans module artifacts with the reversible `eav1_<hex-module>__0` basename.
Text uses `.ll`, bitcode `.bc`, and objects use the target-selected extension. Native
paths preserve Unicode; links, input aliases and duplicate destinations are rejected.
The publisher writes and closes a whole batch in a private directory before replacing
files. Windows uses MoveFileExW replacement; POSIX uses rename. A publication failure
can leave earlier complete files replaced: this is not a multi-file transaction.

Step 34 parses the compilation command options. `--emit` accepts `obj`, `llvm-ir`,
and `llvm-bc`; `--artifact-dir` requires emission. Value options and optimization
levels may appear only once. O0 is the default; O2 selects speed policy, with
`--no-type-specialization` overriding it in either order. Compilation switches
conflict with frontend actions and project creation; explicit emission conflicts
with executable `--output`. The following integration steps consume this policy.

Step 35 connects positional source batches to the real backend. Default commands
lower, specialize under the selected policy, optimize, verify and emit native objects
in memory. `--emit obj|llvm-ir|llvm-bc` publishes the whole successful batch under
`build/aot` or `--artifact-dir`. Parse/semantic/target failures publish nothing.
LLVM diagnostic callbacks respect opt-in remark filters; warnings/errors remain visible.
For example: `erlangaot -O2 --emit llvm-ir answer.erl client.erl`.

Step 36 uses the same backend for independent selected project targets. Default
artifact roots are manifest-relative `build/aot/<encoded-target>`; explicit roots
are invocation-relative and append the same target component. TOML executable
outputs do not redirect artifacts. All selected targets must compile successfully
before publication begins, and source/manifest aliases are protected across targets.

Step 37 extends `--verbose` with `[comp]` events on stderr. Events retain original
source paths, known module names, project targets and phase order; controls and
delimiters are escaped. Analysis/inference, lowering, specialization decisions,
verification, optimization and emission are reported only when started. Profile
displays are bounded, and disabled/no-benefit/work/growth/variant-limit decisions
are explicit. Frontend-only and informational actions do not produce backend traces.

Step 38 implements `--print-ir` (after compiler specialization, before LLVM passes)
and `--print-optimized-ir` (after the selected verified pipeline). Neither action
emits machine code or files. Both flags print adjacent before/after snapshots per
module, in input/selected-target order. A single snapshot is LLVM assembly; multiple
snapshots have escaped LLVM-comment headers and must be separated before assembly.
Use `--emit llvm-ir` for individual machine-consumable files. Preprocessing, target,
optimization and specialization options are allowed; frontend actions, project
creation and all emission/output destinations conflict. Traces remain on stderr.

Step 39 implements `--print-types` through the shared semantic pipeline, stopping
before LLVM state, target setup or lowering. Reports follow input module and
selected-target order; functions and expressions retain logical source locations.
Declared aliases, opaque/nominal identities, callbacks, specs and record metadata
stay separate from inferred inputs/results and exact parameter relations. Unknown
implementation facts are explicit `term() [unknown]`; declared specs never narrow
them. Graph widening and bounded display truncation are visible. Recursive aliases
remain symbolic references. Diagnostics and optional traces stay on stderr;
reports go to stdout and are not a public stage-input serialization format.

```sh
erlangaot --print-types answer.erl client.erl
erlangaot --print-types --project project.toml --target demo --verbose
```

Validation through step 39 (2026-09-29): fresh Windows x64 Debug compiler/runtime
build, 93/93 CTests with zero skips, full Lizard and clang-tidy over 180 production
translation units, formatting and whitespace checks pass. Native execution evidence
remains Windows x64; other native hosts and full frontend sanitizer coverage remain
pending. Steps 40–46 have not been started in this implementation batch.

Step 40 (2026-09-29): Public CLI objects execute in a separately configured Clang harness through the mandatory runtime link target at O0/O2; integer/immediate boundaries, projection and nested calls, ABI rejection, missing-runtime failure and explicit teardown pass. Fresh Windows x64 Debug compiler/runtime build: 95/95 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.

Step 41 (2026-09-29): 150 seeded/fixed calls agree with OTP and an independent evaluator across four optimization/specialization modes, repeated twice; annotated/unannotated pairs and incorrect specs preserve behavior. CRLF and CMake native-path issues in the new test were fixed before the passing gate. Fresh Windows x64 Debug compiler/runtime build: 96/96 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.

Step 42 (2026-09-29): Cost records cover source and synthetic guards at O0/O2 with specialization disabled/enabled. High-arity wide-union inputs remain generic, O2 outputs match byte-for-byte, 3/32/128 caps and 2x growth hold, and native guard/fallback results agree. Timings are descriptive only. Fresh Windows x64 Debug compiler/runtime build: 97/97 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.

The `codegen_cross_targets` test emits through the CLI and invokes the selected
SDK's `llvm-readobj --file-headers --symbols` and `llvm-nm` on every object.
Its O0/O2 matrix covers Linux i686/x86_64/armv7/aarch64, Windows i686/x86_64,
and arm64 Apple macOS. It checks target formats, architectures, term widths,
encoded small-integer endpoints, descriptor ABI v1, registration symbols and
cross-module/runtime imports. Target overflow, unknown architectures and
unavailable backends fail without artifacts. SDKs lacking a supported backend
must report that absence rather than substitute a host target.

These are object/IR inspection results. Current native generated-code execution
is Windows x64 only; native Linux, Apple Silicon and 32-bit runtime/ABI execution
remain pending. Historical macOS runtime skeleton results are not evidence for
the current CLI-generated native harness.

Step 43 (2026-09-29): CLI-emitted objects pass SDK readobj/nm inspection for seven ELF, Mach-O and COFF targets at O0/O2, including architecture, exports/imports, runtime references, ABI widths/tags, exact integer endpoints and failure without publication. Foreign native execution remains pending. Fresh Windows x64 Debug compiler/runtime build: 98/98 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.

Step 44 (2026-09-29): All compiler catalog families are audited through both CLI modes at O0/O2 with verbosity on/off. Explicit executable output now fails instead of silently succeeding. Native allocation rejection preserves generated calls, heap accounting and clean teardown; atom collection is documented as a reservation without an owner. Fresh Windows x64 Debug compiler/runtime build: 99/99 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.

Compilation budgets are per target: 1,024 modules, 250,000 owned AST nodes per
module and 1,000,000 per batch. Counts include forms, expressions, patterns, literal
terms and declared types. The frontend checks counts while retaining inputs;
semantic/backend admission checks them again for internal callers. Defaults are
internal policy, not new CLI switches. Rejection is an ordinary resource diagnostic.

LLVM text, bitcode, objects and inspection snapshots retain at most 64 MiB per
module and 256 MiB per batch. A checked stream latches overflow, discards subsequent
bytes and reports failure after LLVM returns; it does not throw resource-limit
exceptions through the SDK. Buffer allocation failure is contained at the same
boundary. These are serialized-output budgets, not a promise to recover from LLVM
internal bugs or to cap all SDK allocator usage. Failed batches publish nothing.
Existing artifact replacement is complete-file atomic, not a whole-batch transaction.
Injected partial-write, close and interrupted-write exceptions verify owned staging
cleanup and destination preservation. Abrupt process termination can leave a private
staging directory; it cannot publish that partial file as the destination.

Runtime-only Debug validation additionally found MSVC iterator proxies allocating
inside noexcept default container constructors/string moves. Explicit catchable
empty-container construction and publication key/name copies preserve the existing
allocation-failure status/rollback contracts without disabling iterator debugging.

Windows sanitizer setup uses Release probes, `/EHsc /fsanitize=address`, `/MT`, the
installed Clang ASan import library and whole-archive static runtime thunk, with its
DLL directory on PATH. Nested consumers inherit probe configuration and link flags.
See [Clang sanitizer setup](https://clang.llvm.org/docs/AddressSanitizer.html).
The current prebuilt LLVM SDK rejects full compiler ASan linkage because its MSVC
STL `annotate_string=0` conflicts with instrumented code's value 1. No annotation
checks were disabled to bypass that incompatibility; full compiler/frontend ASan,
UBSan and LeakSanitizer remain pending. Runtime-only ASan is validated independently.

Step 45 (2026-09-29): Batch/AST and bounded writer byte ceilings reject without publication; injected partial/close/interrupted writes preserve destinations and clean staging. Debug STL OOM termination paths were repaired without suppressing iterator checks. Compiler-only 80/80, runtime-only Debug 16/16 and runtime ASan 16/16 pass; full compiler ASan remains blocked by the installed SDK annotation ABI. Full quality covers 182 production commands. Fresh Windows x64 Debug compiler/runtime build: 102/102 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.
