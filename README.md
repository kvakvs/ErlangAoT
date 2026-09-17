# ErlangAoT

A C++ ahead-of-time compiler project for Erlang/OTP 29, with a separate C++ runtime.
Currently implemented: CLI argument handling and independent CMake build targets.
Parsing, code generation, and runtime behavior are not implemented yet.

## Build on macOS

Install Apple's Command Line Tools (`xcode-select --install`) and CMake 3.28 or
newer. C++23 is the default. No LLVM development libraries or Erlang installation
are required for this scaffold.

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
      --               End option parsing
```

Quote paths containing spaces. The token following `-o`/`--output` is consumed
as its path, even if it starts with `-`. Options are validated before help/version
is displayed; help takes precedence over version. Valid compilation requests
check that inputs are readable regular files, then report that compilation is not
implemented. No output file is created or overwritten.

Exit codes: `0` for help/version, `2` for command-line usage errors, `1` for input
errors or the unimplemented compilation request. Diagnostics go to stderr.

See [the project plan](00-plan.md) and [future Windows support](.agents/plan-windows.md).
