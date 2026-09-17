# File map

- `CMakeLists.txt`: project/version, build switches, standard validation, CTest.
- `cmake/ProjectOptions.cmake`: target-local C++ standard and warning settings.
- `compiler/CMakeLists.txt`: compiler executable, version define, output location.
- `compiler/src/main.cpp`: CLI parsing/help/version, input validation, exit codes.
- `runtime/CMakeLists.txt`: separate runtime archive target.
- `runtime/src/runtime.cpp`: placeholder translation unit, no runtime behavior.
- `abi/CMakeLists.txt`: reserved interface target; no ABI declarations yet.
- `tests/CMakeLists.txt`: native compiler CLI test registration.
- `tests/cli.cmake`: executable-level CLI checks and output-preservation checks.
- `README.md`: current build, usage, status, and configuration options.
- `.agents/plan-windows.md`: Windows toolchain, ABI, paths, runtime, and CI follow-up.
- `00-plan.md`: preliminary full project structure and deferred design decisions.
- `AGENTS.md`: project intent and agent instructions; `aimemory.md`: working notes.
- `.gitignore`: build trees, local CMake presets, macOS metadata.
