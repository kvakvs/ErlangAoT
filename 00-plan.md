# ErlangAoT preliminary project plan

Status: initial structure and build plan, 2026-09-17. This document proposes the
layout; the directories, CMake files, and commands below are not implemented yet.

## Scope and decisions

- Implement the Erlang preprocessor, parser, and ahead-of-time compiler in C++.
- Implement the runtime separately in C++, in the same repository.
- Use CMake with two principal build targets: the compiler executable and the
  runtime library. Tests may introduce additional internal targets.
- Target Erlang/OTP 29 language behavior. An installed OTP can serve as a test
  oracle; using its frontend is not required by this C++ implementation plan.
- Generate LLVM-compatible output and link generated program code with the
  runtime. The output representation, LLVM integration method, and LLVM version
  remain separate decisions.

The compiler is a host tool. The runtime executes on the generated program's
target platform. A native build can build both together; cross-compilation will
use separate build trees for the host compiler and target runtime.

## C++ and build baseline

Use **C++23 by default**, with an explicit C++26 build option for supported
toolchains. This is a practical starting baseline above C++20, not a claim that
C++23 is the newest standard mode. Move the default forward when the selected
compiler and standard library support the features the project needs. Clang's
[feature support table](https://clang.llvm.org/cxx_status.html) documents remaining
gaps in newer modes; enabling a mode alone does not guarantee every feature.

Start with CMake 3.28 or newer and Clang/clang++ as the reference toolchain.
Use a newer CMake if required for a particular compiler's C++26 mode.
Set `CXX_STANDARD` on project targets to the selected standard,
`CXX_STANDARD_REQUIRED` to `ON`, and `CXX_EXTENSIONS` to `OFF`. Unsupported modes
must fail configuration rather than silently fall back. See the
[CMake standard property](https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html).

Prefer ordinary headers and source files initially. C++ modules are not required.
Check specific language/library facilities when first adopting them. Keep warning
and sanitizer settings local to project targets, and list source files explicitly.

## Proposed directory layout

```text
ErlangAoT/
├── AGENTS.md
├── aimemory.md
├── 00-plan.md
├── README.md
├── CMakeLists.txt                 # Root project, options, subdirectories
├── CMakePresets.json              # Shared debug/release configurations
├── .gitignore                    # Build trees, local presets, generated outputs
├── cmake/
│   ├── ProjectOptions.cmake      # Standard, warnings, optional sanitizers
│   └── toolchains/               # Future platform/cross-compilation settings
├── abi/
│   ├── CMakeLists.txt            # Header-only erlang_aot_abi interface target
│   └── include/erlang_aot/abi/    # Generated-code/runtime contract declarations
├── compiler/
│   ├── CMakeLists.txt            # erlang_aot executable
│   ├── include/erlang_aot/compiler/
│   │                            # Cross-component compiler headers
│   └── src/
│       ├── main.cpp              # CLI entry point
│       ├── driver/               # Options, pipeline, external tools, linking
│       ├── source/               # Source buffers, paths, locations
│       ├── diagnostics/          # Errors, warnings, source excerpts
│       ├── lexer/                # Erlang tokens and lexical rules
│       ├── preprocessor/         # Macros, includes, conditional directives
│       ├── parser/               # Erlang source grammar and AST construction
│       ├── ast/                  # Erlang syntax representation
│       ├── semantic/             # Names, scopes, validation
│       ├── ir/                   # Compiler-owned intermediate representations
│       ├── lowering/             # AST/IR transformations
│       ├── codegen/              # Output generation; backend choice deferred
│       └── stage_readers/        # Reserved; no readers implemented initially
│           ├── preprocessed/
│           ├── abstract/
│           └── ir/
├── runtime/
│   ├── CMakeLists.txt            # erlang_runtime library
│   ├── include/erlang_aot/runtime/
│   │                            # Public embedding/startup API, if needed
│   └── src/
│       ├── startup/              # Runtime initialization and shutdown
│       ├── term/                 # Values, atoms, numbers, aggregate terms
│       ├── memory/               # Process heaps, allocation, garbage collection
│       ├── process/              # Process state, lifecycle, links/monitors
│       ├── scheduler/            # Runnable queues, workers, dispatch/yield loop
│       ├── message/              # Mailboxes, send, selective receive
│       ├── event/                # Event loop, timers, asynchronous I/O wakeups
│       ├── bif/                  # Native built-in/basic library functions
│       └── platform/             # OS threading, polling, low-level context code
├── tests/
│   ├── CMakeLists.txt            # CTest registration and test executables
│   ├── compiler/                # Lexer, preprocessing, parsing, lowering tests
│   ├── runtime/                 # Runtime component tests
│   ├── integration/             # Compile, link, run; compare with OTP
│   └── fixtures/                # Erlang sources, headers, expected diagnostics
├── examples/                    # Small compilable Erlang programs
├── docs/                        # Future design and compatibility documents
└── build/                       # Ignored, out-of-source build trees
```

Create directories as their components are introduced. Internal headers used by
one component stay alongside its sources; headers shared between compiler
components belong under `compiler/include/`. The runtime follows the same rule.
Avoid a generic shared utility library until a concrete need appears.

`stage_readers/` reserves source locations only. Stage serialization formats,
reader APIs, and whether stages exchange text or parsed data are intentionally
undecided at this stage.

## Targets and dependencies

| Target           | Kind                                 | Purpose and dependencies                                                                      |
| ---------------- | ------------------------------------ | --------------------------------------------------------------------------------------------- |
| `erlang_aot`     | Executable, output name `erlang-aot` | Compiler components; ABI declarations; LLVM libraries only if the selected backend needs them |
| `erlang_runtime` | Static library initially             | Runtime components; ABI declarations; OS dependencies such as `Threads::Threads`              |
| `erlang_aot_abi` | CMake `INTERFACE` target             | Shared include path and contract headers; produces no binary                                  |

The compiler executable does not link the runtime library into itself. Its driver
locates the runtime when linking an Erlang application. The runtime does not
depend on compiler internals or LLVM libraries. LLVM library discovery, if needed,
belongs inside the compiler build so runtime-only builds remain independent.

The generated-code/runtime boundary may use any explicitly supported ABI for the
target platform, including C++ linkage. C linkage is not a requirement. Keep the
contract in `abi/`; its exact form should follow the chosen code-generation path:

- If emitting C++, generated code can include runtime interface headers and call
  ordinary C++ functions. Clang then handles name mangling, argument/return
  lowering, and any C++ object-layout rules used by the interface.
- If emitting LLVM IR directly, the compiler must generate matching symbol names,
  calling conventions, argument/return representations, and ABI attributes.
  A small C-linkage interface implemented in C++ is a simpler initial option here;
  direct C++ ABI calls are also possible if their lowering is implemented or
  delegated to Clang. LLVM does not infer a C++ ABI from source-level declarations.
- Specialized conventions for calls between generated Erlang functions can be
  considered separately from calls into the C++ runtime.

Matching architecture and OS is necessary but insufficient. Both sides must agree
on the target ABI/environment, data layout, calling conventions, and relevant
toolchain settings. If C++ objects or exceptions cross the boundary, their layout,
lifetime, exception/unwind rules, and any exposed standard-library ABI must also
match. See the [C++ ABI specification](https://itanium-cxx-abi.github.io/cxx-abi/abi.html),
[LLVM calling conventions](https://llvm.org/docs/LangRef.html#calling-conventions),
and [Clang toolchain guidance](https://clang.llvm.org/docs/Toolchain.html).

Initially build generated code and the runtime with one selected compatible target
toolchain/configuration, and rebuild them together when the contract changes.
Cross-toolchain or cross-version binary compatibility is not an initial promise.
Term layout, GC coordination, exception policy, and ABI versioning remain later
design decisions. C++ linkage by itself supplies neither Erlang semantics nor a
performance advantage over C-linkage functions implemented in C++.

Initially produce a static runtime archive (`.a`, or `.lib` on Windows). A shared
runtime can be added later. Dynamic Erlang code loading/upgrades are a separate
feature decision, not a consequence of choosing a shared runtime library.

Generated application entry code calls the runtime startup interface; it provides
the executable entry point. The runtime library is not itself a runnable program.
Pure Erlang library modules can later have a separate source location when their
compilation and packaging are planned; `runtime/src/bif/` holds native functions.

## CMake organization and proposed commands

The root CMake project declares C++ and options, adds `abi/`, conditionally adds
`compiler/` and `runtime/`, and enables tests through `include(CTest)`.

Proposed cache options:

- `ERLANG_AOT_BUILD_COMPILER=ON`
- `ERLANG_AOT_BUILD_RUNTIME=ON`
- `ERLANG_AOT_CXX_STANDARD=23` (accepted initial values: `23`, `26`)
- `BUILD_TESTING=ON` (standard CTest option)

Component tests follow the enabled components. Integration tests require both
components and executable target binaries, or an explicitly configured emulator.
OTP comparison tests require a configured OTP 29 installation.

After scaffolding, a native build should support:

```sh
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build/debug --target erlang_aot
cmake --build build/debug --target erlang_runtime
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

A runtime-only configuration should support:

```sh
cmake -S . -B build/runtime -DERLANG_AOT_BUILD_COMPILER=OFF -DBUILD_TESTING=OFF
cmake --build build/runtime --target erlang_runtime
```

Use the corresponding runtime-off option for a compiler-only configuration.
For cross-compilation, build the compiler natively with runtime disabled, then
configure the runtime in a different build tree using a CMake toolchain file.
The compiler driver will need a way to select that target runtime and toolchain.

## Compiler flow and diagnostics

Logical flow: source loading → lexing/preprocessing → parsing → semantic analysis
→ lowering → code generation → external compilation/linking with the runtime.
Lexing and preprocessing cooperate; this does not imply preprocessing raw text
without tokens or committing to an intermediate exchange format.

Provisional CLI name: `erlang-aot`. A future invocation could be
`erlang-aot main.erl -o main`; entry-module/function selection and build-system
integration remain to be specified.

Centralize diagnostics and preserve source locations through transformations,
including macro expansion and include origins. Write diagnostics to stderr,
return nonzero on failure, and distinguish Erlang source errors from external
toolchain failures. CLI details and machine-readable diagnostic formats are later
work.

## Initial implementation sequence

1. Scaffold CMake and the two principal targets; verify combined and independent
   builds using the selected C++ standard.
2. Add source handling, diagnostics, lexer, preprocessor, and parser for a small,
   explicitly documented Erlang subset, with positive and negative fixtures.
3. Establish a minimal ABI and runtime startup/value support, then compile, link,
   and run one small Erlang program through the complete pipeline.
4. Add process scheduling, messaging, event handling, and collection incrementally.
   Test bounded-stack tail recursion, scheduler fairness, selective receive, and
   live-value preservation during GC against the supported OTP behavior.

Before broad source compatibility, resolve parse transforms, required BIFs/OTP
services, native records and other OTP 29 features, and the supported module
loading model. These are compatibility work items, not directory-layout blockers.
