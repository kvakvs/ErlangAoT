# Abstract / Intent of ErlangAoT Project

This project researches and implements a new target language for LLVM (Clang is assumed to be available) which is latest Erlang version 29.

The expected modules this project will have:
- Preprocessor of Erlang
- Parser of Erlang
- Compiler/transformer of Erlang abstract code into LLVM abstract code
- Tests?
- Interface between compiler stages to be either text or internal parsed data format. Intermediate stage parsers can be done later do not plan more than directory source locations for them.

# End Goal

The source code of existing pure Erlang projects will be buildable via LLVM into runnable executables which retain most of Erlang features
(isolated memory, process execution and task switching, message passing, garbage collection etc). The support for dynamic code loading and code upgrades can be implemented as separate dynamic SO/DLL modules, or dropped.

# Questions to Answer

(Replace text here in place as the answers are derived)
- Implementation language: C++ for the preprocessor, parser, compiler tool, and a separately built C++ runtime. Use CMake; preliminary baseline is C++23 with C++26 selectable on supported toolchains.
- Project structure: compiler executable `erlang_aot` (output name `erlang-aot`) and separate runtime library `erlang_runtime`, in one repository. See `00-plan.md` for the preliminary layout, build targets, CLI direction, and diagnostics. Stage data exchange and detailed CLI behavior remain undecided.
- Future directories: `compiler/`, `runtime/`, `abi/`, `cmake/`, `tests/`, `examples/`, and `docs/`; see `00-plan.md` for component locations. Intermediate stage readers have reserved directory locations only.

Use `aimemory.md` for AI notes and memory, this file will not be read by humans.
