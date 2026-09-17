# Architecture

- Required pre-commit `check-quality` target combines Lizard (CCN <=10) and
  clang-tidy (cognitive <=10, analyzer/bugprone/performance; warnings as errors).
  Tools live in ignored `.venv-quality/`; both pass. Normal builds stay independent.
- Clang-tidy uses the full build's compilation database and detected include/SDK
  paths. Use Makefiles/Ninja and enable both compiler and runtime for the gate.

- CMake/C++23 project; C++26 selectable. macOS is the currently validated platform.
- `erlang_aot` builds `erlangaot`, the host compiler CLI. It parses arguments and
  checks inputs; compilation is explicitly unimplemented and never writes output.
- `erlang_runtime` builds a separate static archive with a placeholder translation
  unit. No runtime behavior or public ABI is implemented.
- `erlang_aot_abi` is an empty CMake interface target reserved for shared contracts.
  The compiler does not link the runtime. Neither target currently needs LLVM.
- CTest exercises CLI exit codes, streams, path/argument handling, and preservation
  of outputs. Compiler stages and intermediate readers remain future work.
- `erlang_frontend` is a private static compiler component using standalone
  Boost.Parser 1.90.0 headers. Runtime-only builds do not discover Boost.
  The recursive probe and optional exact-version OTP 29.1 oracle implement step 1.
  Token parsing will use an explicit cursor; Boost's public input is characters.
- `references/otp/` is an ignored, shallow OTP-29.1 source checkout for research;
  `.agents/01-pp-otp-tests.md` indexes upstream tests. It is not a build dependency.
- See `00-plan.md` for intended components and `.agents/plan-windows.md` for Windows work.
