# Abstract / Intent of ErlangAoT Project

This project researches and implements a new target language for LLVM (Clang is assumed to be available) which is latest Erlang version 29.
Supported platforms: Windows (x86 targets), Linux (for x86 and ARM targets), MacOS for Apple Silicon ARM targets.

The expected modules this project will have:
- Preprocessor of Erlang
- Parser of Erlang
- Compiler/transformer of Erlang abstract code into LLVM abstract code
- Tests?
- Interface between compiler stages to be either text or internal parsed data format. Intermediate stage parsers can be done later do not plan more than directory source locations for them.

# Extra Resources

`references/otp` is a gitignored clone of Erlang OTP source repository used to look at tests and other implementation details to comply with.

# End Goal

The source code of existing pure Erlang projects will be buildable via LLVM into runnable executables which retain most of Erlang features
(isolated memory, process execution and task switching, message passing, garbage collection etc). The support for dynamic code loading and code upgrades can be implemented as separate dynamic SO/DLL modules, or dropped.

# Questions to Answer

(Replace text here in place as the answers are derived)
- Implementation language: C++ for the preprocessor, parser, compiler tool, and a separately built C++ runtime. Use CMake; preliminary baseline is C++23 with C++26 selectable on supported toolchains.
- Project structure: compiler executable `erlang_aot` (output name `erlangaot`) and separate runtime library `erlang_runtime`, in one repository. The initial CLI/CMake scaffold is implemented; see `README.md` for current usage and `00-plan.md` for the wider plan. Stage data exchange and compilation behavior remain undecided.
- Future directories: `compiler/`, `runtime/`, `abi/`, `cmake/`, `tests/`, `examples/`, and `docs/`; see `00-plan.md` for component locations. Intermediate stage readers have reserved directory locations only.

Use `aimemory.md` for AI notes and memory, this file will not be read by humans.

## Artifacts Produced

The initial directory structure and implementation plan with technology choices is at `00-plan.md`
- Maintain a compact architecture overview in `.agents/arch.md` update it after major changes. Compact the contents sometimes.
- Maintain a compact file list and overview of which large group of modules does what, and which exact file implements what in `.agents/files.md`, keep this file updated, and compact its contents sometimes.

## When Coding

- IMPORTANT: Document class fields creation intent, what will they be doing. Document function creation intent. Keep comments down to 1-2 lines.
- The code will be read by humans, keep it readable.
- The cyclomatic complexity of new functions and new files must remain low (avoid complex code). Use both Lizard and clang-tidy.
- Before a clean commit, run `cmake --build build/debug --target check-quality` in a freshly configured build with both compiler and runtime enabled. Both Lizard and clang-tidy must pass; do not bypass findings with threshold increases or suppressions merely to pass the gate.
