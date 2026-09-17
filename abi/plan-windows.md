# Windows support — future work

Windows is not yet a supported or tested platform. CMake keeps the compiler and
runtime independent and includes basic MSVC-style warning flags, but that does
not establish Windows support.

- Select and validate MSVC or clang-cl with a Windows SDK, supported C++ standard,
  and CMake generator. Test Debug/Release with multi-configuration generators.
- Select consistent target architecture, C++ ABI, and CRT settings for the runtime
  and generated programs. Decide `/MD` versus `/MT` before exposing ownership
  across library boundaries; do not assume MinGW and MSVC artifacts interoperate.
- Add Unicode command-line/path handling (the initial CLI uses narrow `main`
  arguments), Windows path tests, and safe process invocation for external tools.
- Implement runtime platform facilities: threads, synchronization, event polling
  (for example IOCP), timers, and any context-switch or executable-memory support.
- Design symbol exports/imports if a DLL runtime is introduced. Account for
  `.exe`, `.lib`, and `.dll` naming when compiler linking is implemented.
- Add Windows CI for both independent targets, CLI tests, and eventually generated
  program/runtime integration tests. Audit test execution for cross-builds.

The generated-code/runtime ABI remains undecided; Windows support does not imply
a requirement for either C or C++ linkage.
