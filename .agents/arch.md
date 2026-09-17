# Architecture

- CMake/C++23 project; C++26 selectable. macOS is the currently validated platform.
- `erlang_aot` builds `erlangaot`, the host compiler CLI. It parses arguments and
  checks inputs; compilation is explicitly unimplemented and never writes output.
- `erlang_runtime` builds a separate static archive with a placeholder translation
  unit. No runtime behavior or public ABI is implemented.
- `erlang_aot_abi` is an empty CMake interface target reserved for shared contracts.
  The compiler does not link the runtime. Neither target currently needs LLVM.
- CTest exercises CLI exit codes, streams, path/argument handling, and preservation
  of outputs. Compiler stages and intermediate readers remain future work.
- See `00-plan.md` for intended components and `.agents/plan-windows.md` for Windows work.
