# ErlangAoT
This repository contains two things:

*   Ahead-of-time translator from Erlang (TODO: Decide kernel or core Erlang)
    to LLVM IR using simplified term memory format and a simplified process model.
    Created from the root Cargo project.
*   Runtime library that together with translated file will give a binary executable.
    Created as `.a` static library by running `cargo build` in the `erl_runtime/` directory.

Bonus points for preparing this to work in OS-less environments.
