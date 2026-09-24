# LLVM compilation contract

Status: step 1 contract, frozen 2026-09-24. LLVM lowering, artifact emission and
runtime execution are future steps of [the implementation plan](../.agents/04-compile.md).
Current CLI defaults still preprocess/parse and return without executable output;
the proposed compilation switches below are not implemented yet.

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

## Provisional generated-code ABI v1

This is a private contract to implement and test in step 7, not a claim that current
runtime sketches implement it or that it matches BEAM.

- A term is an unsigned target-pointer-width integer (32 or 64 bits), aligned to
  the target word. Immediate small integers have low four bits `0xf`, matching the
  sketch's primary/secondary small-integer tags. Payload width is `word_bits - 4`;
  the signed range is `[-2^(word_bits-5), 2^(word_bits-5)-1]`. Range-check exact
  source values before encoding with unsigned shift/OR; decode sign explicitly.
- Generated C-convention entries conceptually have signature
  `Term function(ProcessContext*, const Term* arguments)`. Arity is part of the
  resolved identity. Arguments are a borrowed, word-aligned array in source order,
  valid for the call; zero-arity calls may pass null. The context is live and
  runtime-owned, propagated unchanged through direct calls. Callers supply valid
  ABI terms. Returned terms follow context ownership; this subset allocates nothing.
- Symbol names use `eaot_v1_m<hex-module-UTF8>_f<hex-function-UTF8>_a<decimal-arity>`:
  lowercase byte hex, no normalization, canonical decimal without leading zeroes.
  Separate `eaot_v1_register_m<hex-module-UTF8>` names reserve registration entries.
  Exported entries/registration are externally visible; other functions are internal.
  These names specify IR/C symbols before platform mangling.
- Descriptors declare ABI version and term width; runtime registration validates them
  before invocation. Runtime services use C linkage, opaque handles and explicit
  status/error results. No STL types, RTTI protocol or C++ exceptions cross this ABI.
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
