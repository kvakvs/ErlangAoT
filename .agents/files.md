# File map

- `CMakeLists.txt`: project/version, build switches, standard validation, CTest.
- `cmake/ProjectOptions.cmake`: target-local C++ standard and warning settings.
- `cmake/CheckComplexity.cmake`: standalone/target Lizard gate; CCN limit 10.
- `.clang-tidy`: required diagnostic checks and cognitive-complexity limit 10.
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
  proposed files, and OTP compatibility acceptance criteria; not implemented yet.
- `.agents/01-pp-otp-tests.md`: pinned OTP checkout, upstream suite/case inventory,
  implementation lessons, and test execution prerequisites.
- `references/otp/` (ignored): OTP-29.1 source; main preprocessor tests are in
  `lib/stdlib/test/epp_SUITE.erl`, feature tests in `erts/test/erlc_SUITE.erl`.
- `AGENTS.md`: project intent and agent instructions; `aimemory.md`: working notes.
- `.gitignore`: build trees, OTP reference checkout, local CMake presets, macOS metadata.
