# ErlangAoT

An ahead-of-time compiler project for Erlang/OTP 29. Currently supports
preprocessing and syntax parsing; semantic analysis, executable generation and
runtime execution are not yet implemented.

The next compilation milestone is frozen in [docs/compile.md](docs/compile.md),
including its global LLVM SDK prerequisite and provisional ABI.
The separate runtime now supports [startup, context ownership and shutdown](docs/runtime-lifecycle.md),
with a reusable CMake target for linking native consumers.

Validated on macOS Apple Silicon. Linux and Windows validation remains pending.

## Features

- Preprocessing: macros, includes, conditional compilation and language features.
- OTP 29 syntax: expressions, patterns, records, bitstrings, types/specifications,
  control flow and comprehensions.
- Syntax checking, expanded Erlang source output and an indented syntax-tree view.
- Source diagnostics and multiple input files.
- TOML projects with named targets, source discovery, per-target frontend options,
  and annotated starter files.

## Build

Requirements:

- CMake 3.28+ and a C++23-capable compiler.
- Globally installed LLVM 23.1.x (>=23.1.1) C++ SDK for compiler builds;
  see [SDK setup and compilation contract](docs/compile.md). No LLVM download fallback is provided.
- Boost 1.90+ with Boost.Multiprecision for compiler and runtime; the compiler also
  requires Boost.Parser. Multiprecision is header-only and needs no Boost binary library.
- toml++ 3.4.0 for project manifests; see [dependency setup](docs/projects.md#build-dependency).
- Erlang/OTP 29+ for tests (enabled by default). Erlang is not needed to run the
  built tool; configure with `-DBUILD_TESTING=OFF` to build without it.

On macOS:

```sh
xcode-select --install
brew install cmake boost erlang tomlplusplus llvm@23
```

From the repository root:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

All project targets use C++23 and treat compiler warnings as errors.
The executable is `build/debug/bin/erlangaot`. Builds use two parallel jobs;
override with `cmake --build --preset debug --parallel 8`.

Alternatively, use `make build` to build only `erlangaot` and its dependencies,
or `make test` to build and run the full test suite. For a different configuration:

```sh
make test BUILD_DIR=build/release BUILD_TYPE=Release JOBS=4
```

CMake discovers installed Boost and Erlang, including Homebrew installations.
Pass these options when configuring to override defaults:

| Option                                      | Purpose                                                       |
|---------------------------------------------|---------------------------------------------------------------|
| `-DERLANG_AOT_BOOST_ROOT=/path/to/boost`    | Select a Boost installation or full source tree               |
| `-DERLANG_AOT_TOML_ROOT=/path/to/tomlplusplus-3.4.0` | Select the pinned TOML dependency |
| `-DERLANG_AOT_ESCRIPT=/path/to/bin/escript` | Select an Erlang installation; versions below 29 are rejected |
| `-DLLVM_DIR=/global/prefix/lib/cmake/llvm` | Select an existing global LLVM 23.1.x SDK |
| `-DBUILD_TESTING=OFF`                       | Omit tests and their Erlang dependency                        |
| `-DERLANG_AOT_BUILD_COMPILER=OFF`           | Build only the runtime library                                |
| `-DERLANG_AOT_BUILD_RUNTIME=OFF`            | Build only the compiler                                       |

For multi-configuration generators, add `--config Debug` when building and
`-C Debug` when testing. CMake-aware IDEs can open the repository using the
`debug` preset.

Boost and toml++ headers use CMake `SYSTEM` includes, keeping warnings as errors
for project code. On native macOS builds, CMake also marks Homebrew's linked include
directory as `SYSTEM` when its Boost headers resolve to the selected installation.
This keeps inherited flags such as `CXXFLAGS=-I/opt/homebrew/include` from exposing
Boost warnings, including when that flag is already cached by CMake.
For other installations, avoid adding dependency paths through global `-I` flags (including
`CXXFLAGS`): an ordinary include path can take precedence over a dependency's
system path. If CLion reports Boost warnings as errors, remove those global flags
and clear the cached value, for example:

```sh
cmake -S . -B cmake-build-debug -DCMAKE_CXX_FLAGS:STRING=
```

Then reload CMake in CLion. Use the dependency root options above to select
installations instead of adding global include flags.

## Usage

```sh
./build/debug/bin/erlangaot --parse-check examples/project/src/main.erl
./build/debug/bin/erlangaot --print-pp -I include -DDEBUG examples/project/src/main.erl
./build/debug/bin/erlangaot --print-ast examples/project/src/main.erl
```

On macOS, `./run-macos.sh --parse-check examples/project/src/main.erl` builds first
and runs the latest executable, passing all arguments unchanged. It accepts `BUILD_DIR`,
`BUILD_TYPE` and `JOBS` environment overrides.

```text
erlangaot [options] <source.erl>...
  --project <path>        Read a TOML project instead of positional sources
  --target <name>         Select a target; repeat for more (default: all)
  --new-project <filename>  Create an annotated starter; append .toml when needed
  --preprocess-check       Check preprocessing only
  --parse-check            Preprocess and check syntax
  --print-pp               Print expanded Erlang source
  --print-ast              Print an indented syntax tree
  --verbose                Trace ingested filenames to stderr with [pp]/[parse]
  -I, --include <dir>      Add an include directory (last supplied searched first)
  -D, --define <name[=term]>  Define a macro (default value: true)
  --app-dir <app=dir>      Set an include_lib application directory
  --enable-feature <name>  Enable a language feature
  --disable-feature <name> Disable a language feature
  -h, --help              Show all options
  --version               Show version
  --                      Treat remaining arguments as input paths
```

Quote paths containing spaces and macro values containing shell punctuation:

```sh
./build/debug/bin/erlangaot --parse-check -I include '-DVERSION={1,0}' \
  --app-dir myapp=examples/project examples/project/src/main.erl
```

Check modes are silent on success; diagnostics go to stderr. Print modes write
to stdout and can be combined: `--print-pp --print-ast` prints source before the
tree for each input. Adding `--preprocess-check` does not disable parsing requested
by `--parse-check` or `--print-ast`. Errors may leave partial printed output.

With no check/print action, source inputs and `--project` run preprocessing and
parsing, then reach a compilation placeholder. Successful processing returns `0`;
code generation is not implemented, so no executable is written.

`--verbose` prints `[pp] <filename>` for source files and resolved preprocessor
includes, and `[parse] <filename>` when each source enters the parser. Nested and
library includes are traced as they are loaded; inactive includes are skipped.
The parser consumes expanded tokens incrementally, so its trace can precede include
traces. Tracing goes to stderr in every mode, including projects.

Exit codes: **0** for success (including warnings), **1** for source/project errors,
**2** for usage errors or unknown target names.
Each input is processed independently; any source error makes the overall command fail.

Syntax checks do not validate semantics or execute parse transforms. Check/print
modes do not create output files and reject `-o`/`--output`. Requests to generate
an executable currently fail.

See [preprocessing](docs/preprocessor.md), [parser usage](docs/parser.md) and
[validation status](docs/parser-validation.md) for further details.

## Projects

Run the included two-target example:

```sh
./build/debug/bin/erlangaot --parse-check --project examples/project/project.toml
./build/debug/bin/erlangaot --print-ast --project examples/project/project.toml --target app
./build/debug/bin/erlangaot --preprocess-check --project examples/project/project.toml --target tests --target app
```

Create an annotated project in an existing directory:

```sh
mkdir -p build/project-demo
./build/debug/bin/erlangaot --new-project build/project-demo/demo
mkdir -p build/project-demo/src
cp examples/project/src/main.erl build/project-demo/src/main.erl
./build/debug/bin/erlangaot --parse-check --project build/project-demo/demo.toml
```

Creation writes only the requested TOML file and refuses existing destinations.
The starter contains one `app` target using `src`, with all frontend options at
their defaults. Add source files before checking it.

Manifest paths are relative to the TOML file; CLI paths are relative to the
invocation directory. All targets run by default. Repeat `--target` to choose an
ordered subset; repeat selections run once. `sources` supports literal filenames
and `*`, `?`, `**` patterns; `source_dirs` recursively discovers `.erl` files.
Source search paths only locate explicitly listed files. CLI include paths take
precedence, CLI application roots replace matching names, CLI feature settings
apply last, and duplicate macro definitions remain errors.

See [project format and workflows](docs/projects.md) and
[project validation evidence](docs/project-validation.md). Projects support all
four frontend modes; executable generation remains unimplemented.
