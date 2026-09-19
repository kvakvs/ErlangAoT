# ErlangAoT

An ahead-of-time compiler project for Erlang/OTP 29. Currently supports
preprocessing and syntax parsing; semantic analysis, executable generation and
runtime execution are not yet implemented.

Validated on macOS Apple Silicon. Linux and Windows validation remains pending.

## Features

- Preprocessing: macros, includes, conditional compilation and language features.
- OTP 29 syntax: expressions, patterns, records, bitstrings, types/specifications,
  control flow and comprehensions.
- Syntax checking, expanded Erlang source output and an indented syntax-tree view.
- Source diagnostics and multiple input files.

## Build

Requirements:

- CMake 3.28+ and a C++23-capable compiler.
- Boost 1.90+ with Boost.Parser and Boost.Multiprecision.
- Erlang/OTP 29+ for tests (enabled by default). Erlang is not needed to run the
  built tool; configure with `-DBUILD_TESTING=OFF` to build without it.

On macOS:

```sh
xcode-select --install
brew install cmake boost erlang
```

From the repository root:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

The executable is `build/debug/bin/erlangaot`. Builds use two parallel jobs;
override with `cmake --build --preset debug --parallel 8`.

Alternatively, use `make build` or `make test`. For a different configuration:

```sh
make test BUILD_DIR=build/release BUILD_TYPE=Release JOBS=4
```

CMake discovers installed Boost and Erlang, including Homebrew installations.
Pass these options when configuring to override defaults:

| Option                                      | Purpose                                                       |
|---------------------------------------------|---------------------------------------------------------------|
| `-DERLANG_AOT_BOOST_ROOT=/path/to/boost`    | Select a Boost installation or full source tree               |
| `-DERLANG_AOT_ESCRIPT=/path/to/bin/escript` | Select an Erlang installation; versions below 29 are rejected |
| `-DBUILD_TESTING=OFF`                       | Omit tests and their Erlang dependency                        |
| `-DERLANG_AOT_BUILD_COMPILER=OFF`           | Build only the runtime library                                |
| `-DERLANG_AOT_BUILD_RUNTIME=OFF`            | Build only the compiler                                       |
| `-DERLANG_AOT_CXX_STANDARD=26`              | Use C++26 if supported                                        |

For multi-configuration generators, add `--config Debug` when building and
`-C Debug` when testing. CMake-aware IDEs can open the repository using the
`debug` preset.

## Usage

```sh
./build/debug/bin/erlangaot --parse-check src/example.erl
./build/debug/bin/erlangaot --print-pp -I include -DDEBUG src/example.erl
./build/debug/bin/erlangaot --print-ast src/example.erl
```

On macOS, `./run-macos.sh --parse-check src/example.erl` builds first and runs the
latest executable, passing all arguments unchanged. It accepts `BUILD_DIR`,
`BUILD_TYPE` and `JOBS` environment overrides.

```text
erlangaot [options] <source.erl>...
  --preprocess-check       Check preprocessing only
  --parse-check            Preprocess and check syntax
  --print-pp               Print expanded Erlang source
  --print-ast              Print an indented syntax tree
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
  --app-dir myapp=/path/to/myapp src/first.erl src/second.erl
```

Check modes are silent on success; diagnostics go to stderr. Print modes write
to stdout and can be combined: `--print-pp --print-ast` prints source before the
tree for each input. Adding `--preprocess-check` does not disable parsing requested
by `--parse-check` or `--print-ast`. Errors may leave partial printed output.

Exit codes: **0** for success (including warnings), **1** for source/input errors
or unimplemented compilation, **2** for usage errors. Each input is processed
independently; any source error makes the overall command fail.

Syntax checks do not validate semantics or execute parse transforms. Check/print
modes do not create output files and reject `-o`/`--output`. Requests to generate
an executable currently fail.

See [preprocessing](docs/preprocessor.md), [parser usage](docs/parser.md) and
[validation status](docs/parser-validation.md) for further details.
