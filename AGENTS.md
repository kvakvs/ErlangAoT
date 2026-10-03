# Abstract / Intent of ErlangAoT Project

This project researches and implements a new target language for LLVM (Clang is
assumed to be available) which is latest Erlang version 29. Supported platforms:
Windows (x86 targets), Linux (for x86 and ARM targets), MacOS for Apple Silicon
ARM targets.

The expected modules this project will have:
- Preprocessor of Erlang
- Parser of Erlang
- Compiler/transformer of Erlang abstract code into LLVM abstract code
- Tests?
- Interface between compiler stages to be either text or internal parsed data
  format. Intermediate stage parsers can be done later do not plan more than
  directory source locations for them.

# Extra Resources

`references/otp` is a gitignored clone of Erlang OTP source repository used to
look at tests and other implementation details to comply with.

Track the latest official `maint-29` branch, not a fixed release tag or
`master`. At the start of future OTP-dependent work, check upstream and refresh
the pin when the branch advances. Keep `references/otp-pin.cmake`, the checkout,
corpus hashes, grammar evidence and current documentation synchronized; follow
`docs/otp-reference.md`. Preserve historical validation records with their
original revisions. Never silently change the reference during configuration or
tests.

# End Goal

The source code of existing pure Erlang projects will be buildable via LLVM into
runnable executables which retain most of Erlang features (isolated memory,
process execution and task switching, message passing, garbage collection etc).
The support for dynamic code loading and code upgrades can be implemented as
separate dynamic SO/DLL modules, or dropped.

# Questions to Answer

(Replace text here in place as the answers are derived)
- Implementation language: C++23 for the preprocessor, parser, compiler tool,
  and a separately built C++ runtime. Use CMake; all project targets treat
  compiler warnings as errors.
- Project structure: compiler executable `erlang_aot` (output name `erlangaot`)
  and separate runtime library `erlang_runtime`, in one repository. See
  `README.md` for current usage, `.agents/00-finished.md` for completed work and
  the outstanding compiler/runtime checklist. Stages exchange internal owned
  data; public interchange remains deferred.
- Future directories: `compiler/`, `runtime/`, `abi/`, `cmake/`, `tests/`,
  `examples/`, and `docs/`; see `.agents/files.md` for component locations.
  Intermediate stage readers have reserved directory locations only.

Use `aimemory.md` for AI notes and memory, this file will not be read by humans.

## Artifacts Produced

Completed foundations, frontend/project/compiler implementation, runtime
skeleton and test migration are archived in `.agents/00-finished.md`.
- Maintain a compact architecture overview in `.agents/arch.md` update it after
  major changes. Compact the contents sometimes.
- Maintain a compact file list and overview of which large group of modules does
  what, and which exact file implements what in `.agents/files.md`, keep this
  file updated, and compact its contents sometimes.

## When Coding

- Keep APIs project-internal C++23. C-compatible headers, linkage wrappers and
  external interoperability are deferred until there is a concrete need.
- IMPORTANT: Document class fields creation intent, what will they be doing.
  Document function creation intent. Keep comments down to 1-2 lines.
- The code will be read by humans, keep it readable.
- The cyclomatic complexity of new functions and new files must remain low
  (avoid complex code). Use both Lizard and clang-tidy.
- Before a clean commit, run `cmake --build build/debug --target check-quality`
  in a freshly configured build with both compiler and runtime enabled. Both
  Lizard and clang-tidy must pass; do not bypass findings with threshold
  increases or suppressions merely to pass the gate.
- Keep all code clang-formatted (Use either makefile target 'format' or invoke
  clang-format)
- Before commit: All .ERL and terms files must be formatted. Install local copy
  of [erlfmt](https://github.com/WhatsApp/erlfmt) under 'thirdparty/tools/', if
  missing it should be installed. Always run erlfmt on new and modified .ERL and
  terms files which did not have a syntax error in them planted intentionally.

## Testing Strategy

- When testing against golden master, make sure the project owns the fixtures
  used in testing as gold master and that they're generated once from
  Erlang/OTP, but Erlang/OTP should not be required for building and testing
  ErlangAoT.
- Minimize unit testing and maximize meaningful black-box and end-to-end
  coverage. Prefer real Erlang source files and project fixtures exercised
  through the compiler CLI, checking exit status, diagnostics, generated
  artifacts, and executable behavior as those features become available.
- Exercise real compiler stages and runtime integrations together; compare
  observable behavior with Erlang/OTP where appropriate. Avoid mocks and
  assertions tied to private implementation details when a real workflow can
  cover the behavior.
- Add focused unit tests only for important edge cases or invariants that cannot
  be covered reliably or practically through black-box or end-to-end tests.
  Avoid duplicating coverage across layers; preserve existing useful tests
  unless equivalent behavioral coverage replaces them.

