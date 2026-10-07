# Compilation

`erlangaot` compiles Erlang/OTP 29 modules through LLVM to verified IR, bitcode
or native objects. `-o/--output` links positional inputs (or one selected
project target), their [entry](executables.md) startup object and the runtime
into an executable ([linking](executables.md#linking)); project builds link
executable targets to their manifest outputs ([projects](projects.md#executables)). Emitted
objects can also run through a C++ harness linked with the runtime (see the
example below).

## Accepted source subset

Named modules with exports and ordered function clauses. Heads and body matches
accept variables, `_`, aliases, repeated names and patterns over atoms,
arbitrary integers, finite floats, tuples, lists/strings, maps, bitstrings and
ordinary tuple records. Bodies are sequences of matches, constructors, checked
operators/guard BIFs, `erlang:display/1` ([printing](terms.md#printing)),
`erlang:halt/0,1` ([exit status](executables.md#exit-status)), the raising
`error/1,2,3`, `exit/1`, `throw/1` and `erlang:raise/3`
([ABI](abi.md#failure-channel-revision-2)), `case`/`if`, `catch Expr`,
`try ... of ... catch Class:Reason:Stack ... after` with
[stack traces](abi.md#stack-traces), `maybe ... else ... end`, list, binary and map comprehensions
([patterns](patterns.md#comprehensions)) and direct local or
literal remote calls within the batch, including self, mutual and cross-module
recursion on explicit process frames with proper tail calls
([execution model](execution-model.md#implementation)); body recursion is bounded
by the process stack (uncapped by default), not the native stack. Guards support the full admitted
catalog. See [patterns](patterns.md), [guards](guards.md) and [terms](terms.md).

Function values `fun F/A`, `fun M:F/A`, anonymous funs with captured
variables and calls of funs run ([funs](funs.md)). Rejected with diagnostics
even in unused functions: `receive`, named funs, dynamic `M:F(...)` calls and
funs of builtins,
processes and messaging.
Accepted attributes: `module`, `export`, `file`, tuple and native `record`,
`export_record`, `import_record`, type/spec
forms, `doc`/`moduledoc`, `author`, `vsn`, `copyright`, `deprecated`,
`-compile` with `{no_auto_import, ...}` or warning-only `nowarn_*` options (for
example `nowarn_deprecated_catch`) and `-import` of `erlang` guard BIFs. Other
attributes (`on_load`, parse transforms, other
`compile` options, parameterized modules) are rejected.
Sources starting with `#!` follow [escript rules](executables.md#escripts)
(implicit module and `main/1` export, `-mode` accepted).
Type/spec forms are analyzed but never change generated code. Syntax-only modes
(`--parse-check`, `--print-ast`, ...) accept the full grammar.

## Run the compiled-module example

`client:main/1` makes the example a program:

```sh
./build/debug/bin/erlangaot -O2 -o build/demo examples/compile/answer.erl examples/compile/client.erl
./build/demo     # prints 42, -7 and {record,map,binary,list,integer,other}
```

The same modules also run through a C++ harness. From a Windows x64 Developer
PowerShell with the built compiler:

```powershell
$tool = './build/debug/bin/erlangaot.exe'
& $tool -O0 --emit obj --artifact-dir build/example-aot examples/compile/answer.erl examples/compile/client.erl
cmake -S examples/compile -B build/example-native -G Ninja -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Debug "-DGENERATED_DIR:PATH=$((Resolve-Path build/example-aot).Path)"
cmake --build build/example-native
./build/example-native/bin/Debug/compiled_modules.exe
```

It prints `42`, `-7`, `record`, `map`, `binary`, `list`, `integer`, `other`, one
per line. The harness registers modules explicitly, creates a context and
decodes results; it is an example host, not a production entry point. On Unix
use `build/debug/bin/erlangaot`, `clang++` and `-DGENERATED_DIR="$PWD/build/example-aot"`
(native runs there are not yet validated).

Other actions on the same sources:

```powershell
& $tool -O2 --emit llvm-ir --artifact-dir build/example-ir examples/compile/answer.erl examples/compile/client.erl
& $tool --print-types --verbose examples/compile/answer.erl examples/compile/client.erl
& $tool --print-ir --print-optimized-ir examples/compile/answer.erl examples/compile/client.erl
& $tool -O2 --no-type-specialization --verbose examples/compile/answer.erl examples/compile/client.erl
```

## Options

| Option | Behavior |
|---|---|
| `--emit obj\|llvm-ir\|llvm-bc` | Publish one artifact per module |
| `--artifact-dir DIR` | Artifact root (requires `--emit`) |
| `-o PATH` / `--output PATH` | Link an executable ([linking](executables.md#linking)); conflicts with `--emit` |
| `--linker PATH`, `--runtime-library PATH` | Clang driver and runtime archive for `-o` |
| `--entry MODULE[:FUNCTION]` | Executable entry function/1; validated in every compiling mode and adds the `eav1_start` startup artifact ([executables](executables.md#startup-object)) |
| `--target-triple TRIPLE` | Target machine; `--target` is project target selection |
| `-O0` / `-O2` / `-Os` | Default generic code + LLVM O0 / bounded specialization + LLVM O2 / LLVM Os, no specialization, one section per symbol and linker dead-stripping of unreferenced code and data |
| `--no-type-specialization` | Disable variants regardless of option order |
| `--print-ir` / `--print-optimized-ir` | Verified IR before/after LLVM passes, with Erlang source lines as comments |
| `--print-types` | Declared and inferred type report; stops before LLVM |
| `--verbose` | `[pp]`, `[parse]` and `[comp]` phase events on stderr |
| `--impldebug n[,n...]` | Implementation-step debug output on stderr (e.g. `23`: inference summaries) |

- Without `--emit`, compilation verifies objects in memory and writes nothing.
- Positional inputs form one batch; each project target is its own batch.
- Artifact roots: `build/aot` (positional) or `build/aot/<hex-target>` under the
  manifest directory. Explicit roots are invocation-relative.
- Names are reversible hex: `answer` → `eav1_616e73776572__0.obj` (`.o` for
  ELF/Mach-O, `.ll`, `.bc`); the startup object is `eav1_start.obj`.
- All batches compile and stage before publication. Failures publish nothing
  and keep earlier outputs; replacement is atomic per file, not per batch.
- `--emit` conflicts with `-o`; compilation switches conflict with frontend-only
  actions and `--new-project`. `--print-types` rejects target/optimization options.
- IR snapshots are LLVM assembly; multiple snapshots are separated by escaped
  comment headers and are not one parseable module. Use `--emit llvm-ir` for
  tool input. Source-line comments show original text (unexpanded macros) and
  survive optimization through debug locations.

## Backend

- One LLVM context and target machine per batch. Host triple uses host CPU and
  features; foreign triples use the generic CPU. PIC, small code model.
- Backends: X86, ARM, AArch64 (intersection with the SDK). Unknown or missing
  backends fail; there is no host fallback.
- Verification runs before and after optimization; it checks IR validity, not
  Erlang correctness.
- Budgets per target: 1,024 modules, 250,000 AST nodes per module, 1,000,000 per
  batch. Serialized output: 64 MiB per module, 256 MiB per batch. Exceeding a
  budget is an ordinary resource diagnostic.

## LLVM SDK

Stable LLVM **23.1.x**, minimum 23.1.1. LLVM is a host dependency: architecture,
C++ standard library and Windows CRT must match the compiler tool. Project code
keeps exceptions/RTTI; no exception may unwind through LLVM. The runtime never
uses LLVM.

- Discovery searches standard prefixes (`/usr`, `/usr/local`, `/opt/homebrew`,
  `/opt/local`, `/opt/llvm`, Linuxbrew, `/Library/Developer/Toolchains`,
  Program Files on Windows), including versioned layouts. Build trees are rejected.
- `LLVM_DIR` selects an SDK explicitly; invalid selections fail without fallback.
- If none is found, CMake downloads the pinned **23.1.2** archive (SHA-256
  checked) into `thirdparty/` for Windows x64/ARM64, Linux x64/ARM64 or macOS
  ARM64. `ERLANG_AOT_DOWNLOAD_LLVM=OFF` disables downloads.
- A configure-time link probe checks ABI compatibility.

Reference Windows x64 setup (2026-09-29): host clang-cl 23.1.2 in
`C:/Program Files/LLVM/bin`, SDK `thirdparty/clang+llvm-23.1.2-x86_64-pc-windows-msvc`,
Visual Studio 18 x64 tools, Windows SDK 10.0.26100.0, Ninja, `/MT`,
`_ITERATOR_DEBUG_LEVEL=0`. Automatic selection applies the `/MT` and iterator
settings; an explicit `LLVM_DIR` does not, and the link probe then fails.

Historical macOS reference: Homebrew `llvm` 23.1.1_1 (arm64, shared
`libLLVM.23.1.dylib`, assertions off) with AppleClang 21.

Other prerequisites: CMake ≥ 3.28, a C++23 compiler, Boost ≥ 1.90, toml++
3.4.0 and the quality tools. OTP is needed only for opt-in audits and fixture
regeneration.
