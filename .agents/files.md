# File map

- `Makefile`: macOS/Linux `build` and `test` wrappers around CMake and CTest,
  plus `format`/`fmt` for project C++ files; configurable tool paths and options.
- `CMakePresets.json`: shared C++23 Debug configure/build/test settings for CMake,
  Visual Studio, and VS Code, with the compiler database in `build/debug`.
- `.vscode/{settings,extensions}.json`: CMake Tools IntelliSense provider,
  portable include fallback, and recommended VS Code extensions.
- `CMakeLists.txt`: project/version, build switches, standard validation, CTest.
- `cmake/ProjectOptions.cmake`: target-local C++ standard and warning settings.
- `cmake/CompilerDependencies.cmake`: local/installed pinned Boost.Parser headers.
- `compiler/include/erlang_aot/compiler/probe.hpp`, `compiler/src/parsing/probe.cpp`:
  owned probe result and recursive Boost.Parser grammar; token-iterator constraint.
- `tests/compiler/probe.cpp`: integration, offsets, malformed input and rollback.
- `tests/compiler/preprocessor/oracle.{escript,cmake}`: optional OTP 29.1 epp events;
  `tests/fixtures/preprocessor/{good,bad}.erl`: initial success/error fixtures.
- `cmake/CheckComplexity.cmake`: standalone/target Lizard gate; CCN limit 10.
- `.clang-tidy`: required diagnostic checks and cognitive-complexity limit 10.
- `.clang-format`: four-space LLVM-based formatting with K&R braces, 120 columns,
  and blank lines between type/function definition blocks.
- `cmake/CheckClangTidy.cmake`: project translation-unit analysis and failure propagation.
- `cmake/QualityToolchain.cmake.in`: configured include/SDK paths for clang-tidy.
- `tools/requirements-quality.txt`: pinned Lizard, clang-tidy, and Python dependencies;
  installed locally in ignored `.venv-quality/`.
- `compiler/CMakeLists.txt`: compiler executable, version define, output location.
- `compiler/src/main.cpp`: CLI argument traversal, named-option/output parsing,
  help/version, input validation, exit codes; functions meet both complexity limits.
- `runtime/CMakeLists.txt`: separate runtime archive target.
- `runtime/src/runtime.cpp`: placeholder translation unit, no runtime behavior.
- `abi/CMakeLists.txt`: reserved interface target; no ABI declarations yet.
- `tests/CMakeLists.txt`: native compiler CLI test registration.
- `tests/cli.cmake`: executable-level CLI checks and output-preservation checks.
- `README.md`: current build, usage, status, and configuration options.
- `.agents/plan-windows.md`: Windows toolchain, ABI, paths, runtime, and CI follow-up.
- `00-plan.md`: preliminary full project structure and deferred design decisions.
- `.agents/01-pp.md`: ordered Boost.Parser preprocessor plan, feature coverage,
  proposed files, and OTP compatibility acceptance criteria; steps 1–3 implemented.
- `.agents/01-pp-otp-tests.md`: pinned OTP checkout, upstream suite/case inventory,
  implementation lessons, and test execution prerequisites.
- `references/otp/` (ignored): OTP-29.1 source; main preprocessor tests are in
  `lib/stdlib/test/epp_SUITE.erl`, feature tests in `erts/test/erlc_SUITE.erl`.
- `AGENTS.md`: project intent and agent instructions; `aimemory.md`: working notes.
- `.gitignore`: build trees, OTP reference checkout, local CMake presets, macOS metadata.
- `compiler/include/erlang_aot/compiler/{source,diagnostic,lexer}.hpp`: shared
  source, diagnostic, and internal token/scanner contracts.
- `compiler/src/source/source.cpp`: UTF-8/Latin-1 decoding and byte/position mapping.
- `compiler/src/diagnostics/diagnostic.cpp`: lexical errors and source rendering.
- `compiler/src/lexer/{lexer,numbers,literals}.cpp`: incremental lexical categories,
  unbounded integers/based floats, escaped/verbatim strings and OTP 29 sigils.
- `tests/compiler/{lexer,lexer_dump}.cpp`: scanner checks and private token dump.
- `tests/compiler/preprocessor/{scan.escript,scan.cmake,golden.cmake}`: optional
  live scanner comparison and offline pinned token fixture checks.
- `tests/fixtures/preprocessor/lexical/`: six inputs and OTP 29.1 token/location
  records; provenance/serialization documented in the parent README.
- `compiler/include/erlang_aot/compiler/{directive,preprocessor}.hpp`: directive
  operand values, isolated session state, and private compiler event interface.
- `compiler/src/preprocessor/cursor.hpp`: bounded, category-aware token cursor.
- `compiler/src/preprocessor/directives.cpp`: transactional directive envelopes;
  raw replacement bodies and deferred conditions/terms, no directive effects.
- `compiler/src/preprocessor/preprocessor.cpp`: lexical form driver, passthrough,
  line-leading misplaced-directive heuristic, latched errors, and recovery.
- `tests/compiler/preprocessor/forms.cpp`: syntax, recovery/progress, ordinary
  token preservation, lifetime, and session isolation checks.

Preprocessor progress: steps 4–7 now have semantic implementation and focused tests.
Shared literal-term parsing supports initial macro values; subsequent commits connect later directives.
