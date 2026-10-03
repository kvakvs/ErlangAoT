# Compilation

`erlangaot` compiles Erlang/OTP 29 modules through LLVM to verified IR, bitcode
or native objects. Linking executables is not implemented yet: explicit
`-o/--output` validates the [entry](executables.md) and then fails with
`[executable linking] notimpl`. Generated objects run
today through a C++ harness linked with the runtime (see the example below).

## Accepted source subset

Named modules with exports and ordered function clauses. Heads and body matches
accept variables, `_`, aliases, repeated names and patterns over atoms,
arbitrary integers, finite floats, tuples, lists/strings, maps, bitstrings and
ordinary tuple records. Bodies are sequences of matches, constructors, checked
operators/guard BIFs and direct local or literal remote calls within the batch.
Guards support the full admitted catalog. See [patterns](patterns.md),
[guards](guards.md) and [terms](terms.md).

Rejected with diagnostics even in unused functions: recursion (the call graph
must be acyclic), `case`/`if`/`maybe`/`receive`, `try`/`catch`, funs and
closures, dynamic calls, comprehensions, record updates and native records,
processes and messaging. Accepted attributes: `module`, `export`, `file`,
ordinary `record`, type/spec forms, `doc`/`moduledoc`, `author`, `vsn`,
`copyright`, `deprecated`, `-compile({no_auto_import, ...})` and `-import` of
`erlang` guard BIFs. Other attributes (`on_load`, parse transforms, other
`compile` options, parameterized modules) are rejected.
Type/spec forms are analyzed but never change generated code. Syntax-only modes
(`--parse-check`, `--print-ast`, ...) accept the full grammar.

## Run the compiled-module example

From a Windows x64 Developer PowerShell with the built compiler:

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
| `--entry MODULE[:FUNCTION]` | Executable entry function/1; validated in every compiling mode ([executables](executables.md)) |
| `--target-triple TRIPLE` | Target machine; `--target` is project target selection |
| `-O0` / `-O2` | Default generic code + LLVM O0 / bounded specialization + LLVM O2 |
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
  ELF/Mach-O, `.ll`, `.bc`).
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
