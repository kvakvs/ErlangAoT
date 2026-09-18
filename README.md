# ErlangAoT

A C++ ahead-of-time compiler project for Erlang/OTP 29, with a separate C++ runtime.
The compiler implements OTP 29.1 preprocessing: macros, conditional compilation,
includes, contextual macros, feature configuration, and diagnostic directives.
`--preprocess-check` validates modules without an Erlang installation or output files.
Full Erlang parsing, code generation, and runtime behavior remain unimplemented.

## Build on macOS and Linux

On macOS, install Apple's Command Line Tools (`xcode-select --install`). On Linux,
install a C++23-capable compiler and GNU Make. Both need CMake 3.28 or newer.
C++23 is the default. No LLVM development libraries or Erlang installation are
required for this scaffold.

Compiler builds use Boost.Parser and Boost.Multiprecision from Boost 1.90.0.
Install the standalone parser and the full release's headers:

```sh
git clone --depth 1 --branch boost-1.90.0 https://github.com/boostorg/parser.git build/deps/boost-parser
curl -L https://archives.boost.io/release/1.90.0/source/boost_1_90_0.tar.bz2 -o build/deps/boost_1_90_0.tar.bz2
tar -xjf build/deps/boost_1_90_0.tar.bz2 -C build/deps boost_1_90_0/boost boost_1_90_0/LICENSE_1_0.txt
```

The full archive's SHA-256 is
`49551aff3b22cbc5c5a9ed3dbc92f0e23ea50a0f7325b0d198b705e8ee3fc305`.
The parser commit is `647cec66831407742a6ad78582f2a9f3cd7d44d3`.
Alternatively set `ERLANG_AOT_BOOST_PARSER_ROOT` and `ERLANG_AOT_BOOST_ROOT` to
local source/install prefixes; both can point to one full Boost 1.90.0 installation.
CMake verifies the parser header and arithmetic release. No dependency is downloaded
at configure time, and runtime-only builds do not discover Boost. Both dependencies
are header-only and private to the compiler. Boost.Parser uses its standalone mode.

For Visual Studio 2022 or VS Code, open the repository root as a CMake project.
The checked-in `CMakePresets.json` selects C++23 and `build/debug`; configure it
after installing the Boost headers above. In VS Code, install the recommended
C/C++ and CMake Tools extensions. CMake Tools supplies IntelliSense with the
actual compiler, C++ standard, and include paths, including Boost. If the editor
still shows old errors after the first configure, run **CMake: Configure** and
**C/C++: Reset IntelliSense Database**. A developer using a different Boost
installation can set both Boost root variables in an ignored
`CMakeUserPresets.json`; the shared files contain no host-specific paths.

The same configuration works from a terminal:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

CTest's optional oracle tests require exact OTP 29.1 via `ERLANG_AOT_ESCRIPT`;
they explicitly skip if unavailable. Scanner and expanded-token golden tests use
checked-in records and run without Erlang. `preprocessor_semantics` checks owned
sources, error recovery, source order, includes, expression semantics, locations,
and bounded generated inputs. `preprocessor_oracle` compares expanded tokens and
diagnostic event order with epp, including real OTP headers.
See [preprocessor behavior and validation](docs/preprocessor.md).

After installing the Boost headers above, use the root Makefile:

```sh
make build
make test
make format # or: make fmt
```

`build` configures and builds the compiler, runtime, and test executables in
`build/debug`. `test` builds first and runs CTest, showing failures. Override
`BUILD_DIR`, `BUILD_TYPE`, or `CMAKE_ARGS` when needed, for example:

```sh
make test BUILD_DIR=build/release BUILD_TYPE=Release CMAKE_ARGS='-DCMAKE_CXX_COMPILER=clang++'
```

`format` and `fmt` apply the repository's `.clang-format` to C++ files under
`compiler/`, `runtime/`, `abi/`, and `tests/`. On macOS, the Makefile also finds
Apple's `clang-format` through `xcrun`. Set `CLANG_FORMAT` to another executable
path if needed.

Builds use two parallel jobs by default; set `JOBS=4` to change this. The equivalent
direct CMake commands remain available:

```sh
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build/debug --parallel
ctest --test-dir build/debug --output-on-failure
./build/debug/bin/erlangaot --help
./build/debug/bin/erlangaot --version
```

The default single-configuration build creates `build/debug/bin/erlangaot` and
`build/debug/lib/liberlang_runtime.a`. Multi-configuration generators add a
configuration subdirectory; pass `--config Debug` when building and `-C Debug`
when testing. The runtime archive is a placeholder with no public API yet.

Build either target separately with `--target erlang_aot` or
`--target erlang_runtime`. To omit a component at configuration time, set
`-DERLANG_AOT_BUILD_COMPILER=OFF` or `-DERLANG_AOT_BUILD_RUNTIME=OFF`.
Disable tests with `-DBUILD_TESTING=OFF`. C++26 can be selected with
`-DERLANG_AOT_CXX_STANDARD=26` when supported by the chosen toolchain.

CMake uses the host macOS architecture by default. Set `CMAKE_OSX_ARCHITECTURES`
and `CMAKE_OSX_DEPLOYMENT_TARGET` explicitly when needed; runtime and generated
programs will need compatible target settings. Only native macOS builds have
been validated so far.

## CLI

```text
erlangaot [options] <source.erl>...
  -h, --help           Show help
      --version        Show version
  -o, --output <path>  Future executable output path (default: a.out)
      --preprocess-check  Preprocess and report diagnostics; write no output
  -I, --include <dir>  Include directory (last supplied searched first)
  -D, --define <name[=term]>  Initial macro (default value: true)
      --app-dir <app=dir>  Explicit include_lib application directory
      --enable-feature <name>  Enable a baseline feature (or all)
      --disable-feature <name>  Disable a baseline feature (or all)
      --               End option parsing
```

```sh
./build/debug/bin/erlangaot --preprocess-check -I include -DDEBUG \
  '-DVERSION={1,0}' --app-dir myapp=/path/to/myapp src/example.erl
```

Quote paths and Erlang terms for your shell. Joined `-Ipath` and `-DNAME=TERM`
spellings are supported. Duplicate macro definitions are errors; application mappings
use the last value; feature options apply in order. Each input has an isolated session;
all inputs are processed and any error makes the request fail. Warnings alone succeed.
`--output` conflicts with `--preprocess-check`. No executable is created or overwritten.

Other compilation requests validate readable inputs, then report the unimplemented
backend. Options are validated before help/version; help takes precedence over version.
Exit codes: `0` for help/version or successful preprocessing, `2` for usage errors,
and `1` for input/preprocessing errors or unimplemented compilation. Diagnostics use stderr.

See [the project plan](00-plan.md) and [future Windows support](.agents/plan-windows.md).

## Required quality checks

[Lizard](https://github.com/terryyin/lizard) is an MIT-licensed cyclomatic
complexity analyzer. Version 1.24.0, clang-tidy 22.1.8, and dependencies are pinned in
`tools/requirements-quality.txt`. It analyzes source without compiling or
requiring Clang/LLVM headers. Its lexical measurements are a review aid;
template-heavy and newer C++ constructs can require manual interpretation.

Install into the ignored project environment (Python 3.9+):

```sh
python3 -m venv .venv-quality
.venv-quality/bin/python -m pip install -r tools/requirements-quality.txt
```

On Windows, use `py -3 -m venv .venv-quality` and
`.venv-quality\Scripts\python.exe -m pip install -r tools/requirements-quality.txt`.
The clang-tidy package supplies a native executable for supported wheel platforms.
If unavailable for a host, install LLVM's clang-tidy separately and set
`CLANG_TIDY_EXECUTABLE` when invoking the standalone script below.

**Both tools must pass before a clean commit.** From the project root:

```sh
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ \
  -DERLANG_AOT_BUILD_COMPILER=ON -DERLANG_AOT_BUILD_RUNTIME=ON
cmake --build build/debug --target check-quality --parallel
```

Use a Makefiles or Ninja generator, which produces `compile_commands.json`.
Reconfigure after changing source lists or build options. The combined target
requires both compiler and runtime enabled and fails when either tool fails.
It is the required pre-commit command; Git hooks are not installed automatically.

[Clang-tidy](https://clang.llvm.org/extra/clang-tidy/) uses `.clang-tidy` to enable
static analyzer, bug-prone code, performance, and cognitive-complexity checks.
All reported warnings are errors; the per-function cognitive limit is **10**.
This complements Lizard's cyclomatic metric. It uses the compilation database and
CMake's detected implicit includes/Apple SDK to analyze configured project
translation units and their included project headers. Unreferenced headers are
not standalone translation units. System-header findings are excluded.

Run clang-tidy separately with `cmake --build build/debug --target check-clang-tidy`
or `cmake -DQUALITY_BUILD_DIR=build/debug -P cmake/CheckClangTidy.cmake`.
For a separate installation, pass `-DCLANG_TIDY_EXECUTABLE=/path/to/clang-tidy`
before `-P`. Normal builds remain independent of the quality-tool installation.

Run the check from the project root, without configuring or building C++:

```sh
cmake -P cmake/CheckComplexity.cmake
```

After configuring CMake, the same check is available as:

```sh
cmake --build build/debug --target check-complexity
```

The initial per-function limit is **CCN 10**; a value above 10 makes the command
fail for local checks or CI. Lizard's auxiliary defaults also flag function
length above 1000 lines and more than 100 parameters. Reports include per-file
averages; there is no separate aggregate file threshold yet. The scan covers C++
under `compiler/`, `runtime/`, and `abi/`, excluding the OTP reference checkout and
build/dependency directories. Normal builds do not run this optional target.

For a report that always succeeds despite threshold violations:

```sh
.venv-quality/bin/python -m lizard -l cpp -C 10 -i -1 compiler runtime abi
```

The script accepts `-DQUALITY_PYTHON=/absolute/path/to/python` for another
environment, and `-DCOMPLEXITY_MAX_CCN=N` for threshold experiments; put either
option before `-P`. Use the default threshold for the project quality gate.

The CLI option parser has been split into argument traversal, named-option
handling, and output-operand handling. Its maximum CCN is now **10**, down from
21, and both quality checks pass without suppressions or relaxed thresholds.

## Parser foundation

Phases I–III of the [parser plan](.agents/02-parser.md) are implemented as a native
C++ API consuming expanded preprocessor tokens. The owned typed syntax AST supports
module/file attributes, literals and aggregates, operators/calls, maps, OTP 29
records, bitstrings, restricted pattern syntax, guards, and multi-clause functions. Other syntax remains explicitly
unsupported; no parse-check CLI is exposed yet. See [parser behavior and validation](docs/parser.md) for API contracts,
feature/source ownership, limits, compatibility tests, and remaining phases.
