# LLVM compilation contract

Status: contract frozen 2026-09-24; SDK integration, compilation ownership,
target setup, IR verification, in-memory object emission and the immediate-term ABI
implemented in steps 2–7. Step 8 adds the shared
[deferred-feature catalog and reporting contract](features.md), with separate compiler/runtime
reporters and typed C++ status results; placeholder integration remains later work.
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

Erlang lowering, artifact publication and runtime execution are future steps of [the implementation plan](../.agents/04-compile.md).
Current CLI defaults still preprocess/parse and return without executable output;
the proposed compilation switches below are not implemented yet.

The private backend's `verify_ir` gate checks target consistency, defined function
bodies and whole modules using LLVM's nonfatal verifier APIs. The object emission
entry point calls this gate on the current batch before producing bytes; success
is not cached across mutations. Failures become owned project diagnostics and discard
all staged outputs. Synthetic IRBuilder fixtures cover valid and malformed IR;
verification alone does not establish Erlang semantics or complete compilation.

`emit_objects` uses the SDK's legacy machine-code pass manager and
`TargetMachine::addPassesToEmitFile`, separately from future middle-end optimization.
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

A compatible SDK must already be globally installed. Configuration searches standard
system/package-manager prefixes (including Homebrew); `LLVM_DIR` can choose another
global installation. Repository-local SDKs, build trees and private downloaded or
vendored copies are rejected. Missing/incompatible SDKs are fatal for compiler builds;
Clang alone is insufficient. No configure/build/test helper may download, bootstrap,
install or build LLVM. Runtime-only builds never discover it.

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

These switches are reserved for later implementation, not commands to run today:

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

The full inspection/conflict, ownership, placeholder and validation contracts remain
in [the plan](../.agents/04-compile.md). Each numbered step requires its own full gate
and commit. Cross-platform execution, generated objects and runtime behavior are not
yet validated by this contract.

## SDK integration validation (step 2)

Compiler-enabled configuration now requires the SDK and creates private
`erlang_codegen`/`erlang_llvm_sdk` targets; no new CLI actions are enabled.
LLVM headers/definitions do not propagate to frontend or runtime compilation.
C is enabled for LLVM package dependency probes; project implementations stay C++23.
A configure-time C++23 link probe checks LLVM context/module ABI compatibility,
with RTTI and exceptions retained in project code. `codegen_sdk` also executes
that boundary in CTest.

Automatic discovery searches `/usr`, `/usr/local`, `/opt/homebrew`, `/opt/local`,
`/opt/llvm`, `/home/linuxbrew/.linuxbrew` and `/Library/Developer/Toolchains` on Unix,
and LLVM under Program Files on Windows. Versioned distro and Homebrew layouts
are included. These are global installation roots, not configurable private-copy
fallbacks. Canonical paths reject repository copies and CMake build trees.
`LLVM_DIR` explicitly selects a package beneath those roots; invalid selections
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
missing SDKs, private-only prefixes, explicit private paths, incompatible release
metadata and runtime-only builds. They never install or download dependencies.
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
an explicit next phase; the CLI still uses its placeholder.

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
validation. IR verification, lowering, emission and CLI integration remain deferred.
