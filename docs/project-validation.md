# Project validation

Project support requires C++23. This validation uses that baseline; C++26 checks
are excluded. Executable generation remains unimplemented.

## Host and dependencies

Evidence recorded on 2026-09-20:

| Component | Tested version |
| --- | --- |
| Host | macOS 26.6.2 (25G83), Apple Silicon arm64 |
| Compiler / standard library | Apple Clang 21.0.0 (clang-2100.1.1.101), bundled libc++, C++23 |
| CMake | 4.4.2 |
| Boost | 1.92.0 |
| toml++ | 3.4.0, private header-only dependency |
| Erlang test oracle | OTP 29.0.5, `/opt/homebrew/opt/erlang/bin/escript` |

The full project suite includes bounded TOML loading/schema decoding, wildcard
matching and traversal, physical source deduplication, option precedence,
preflight planning, target execution, CLI conflicts and workflow fixtures.
Creation tests cover native filenames, suffix completion, default output
suffixes, existing destinations, simultaneous creators, and injected write/close
failures. The combined hardening test runs the loader, decoder and invocation
planner over the same 129-source tree and exercises small explicit limits.

Hard links, file/directory symlinks and case aliases are available on the tested
filesystem. Tests conditionally exercise links where native permissions permit;
`project_hardening` prints the observed link/case capabilities. Windows drive,
UNC and backslash assertions are compiled only on Windows. Their presence in the
test source is not evidence that they have run on this host.

## Reproduction

Run from the repository root with the dependencies installed. These commands use
separate build directories so one configuration cannot replace another's binaries
while its tests run.

```sh
cmake --preset debug -DERLANG_AOT_BUILD_COMPILER=ON -DERLANG_AOT_BUILD_RUNTIME=ON
cmake --build --preset debug
ctest --preset debug --no-tests=error --parallel 2
cmake --build build/debug --target check-quality

cmake -S . -B build/project-compiler-only -DCMAKE_BUILD_TYPE=Debug -DERLANG_AOT_BUILD_COMPILER=ON -DERLANG_AOT_BUILD_RUNTIME=OFF
cmake --build build/project-compiler-only --parallel 2
ctest --test-dir build/project-compiler-only --output-on-failure --no-tests=error --parallel 2

cmake -S . -B build/project-runtime-only -DCMAKE_BUILD_TYPE=Debug -DERLANG_AOT_BUILD_COMPILER=OFF -DERLANG_AOT_BUILD_RUNTIME=ON -DERLANG_AOT_TOML_ROOT=/nonexistent/project-validation-toml
cmake --build build/project-runtime-only --parallel 2

cmake -S . -B build/project-sanitize -DCMAKE_BUILD_TYPE=Debug -DERLANG_AOT_BUILD_COMPILER=ON -DERLANG_AOT_BUILD_RUNTIME=ON '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build build/project-sanitize --parallel 2
ctest --test-dir build/project-sanitize --output-on-failure --no-tests=error --parallel 2
```

Runtime-only configuration intentionally uses an absent TOML root to verify that
project dependencies are not discovered. It has no compiler tests.

## Results and remaining hosts

| Configuration | Outcome |
| --- | --- |
| Full C++23 Debug, compiler + runtime | All 64 CTest tests pass |
| C++23 compiler-only Debug | All 64 CTest tests pass |
| C++23 ASan + UBSan, compiler + runtime | All 64 CTest tests pass; no sanitizer diagnostics reported |
| C++23 runtime-only, absent TOML root | Configure and build pass without discovering TOML |

Debug and compiler-only suites were rerun successfully after changing the
byte-limit fixture to binary writes, avoiding Windows newline translation in the
expected byte count. The sanitizer build included that correction. The fresh
full Debug quality gate and formatting results are recorded in the implementation
plan alongside the individual step commits.

Installed Homebrew toml++ 3.4.0 is selected at
`/opt/homebrew/opt/tomlplusplus/include`. Explicit-root precedence, changing a
cached root, and rejecting an absent explicit root were also verified.

| Host | Evidence |
| --- | --- |
| macOS Apple Silicon | Local C++23 results above |
| Linux x86-family | Pending: no host available |
| Linux ARM | Pending: no host available |
| Windows x86-family | Pending: no host available |

The Linux and Windows rows require native runs, including filesystem aliases,
Unicode paths, directory traversal and exclusive creation. No cross-platform
claim is inferred from the macOS run. Filesystem work budgets are finite operation
bounds, not deadlines for blocked OS I/O. Existing parser/oracle coverage and its
separate limitations are recorded in [parser validation](parser-validation.md).
