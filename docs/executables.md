# Executables

Contract for programs built by `erlangaot -o` or a project build. Positional
inputs, or exactly one selected project target, link into the `-o` path; a
project build links each selected executable target to its manifest `output`
([linking](#linking), [projects](projects.md#executables)).

## Entry selection

The entry is an exported function of arity 1 that receives the argument list.

| Source | Spelling | Scope |
| --- | --- | --- |
| CLI | `--entry MODULE[:FUNCTION]` | Positional batch, or the single selected project target |
| Manifest | `entry = "MODULE[:FUNCTION]"` in a `[[targets]]` table | That target |
| Default | Only escript, else only module exporting `main/1` | Only when an executable is requested (`-o`, or a project target with `output`) |

- `FUNCTION` defaults to `main`. Names are unquoted atom text: 1–255 Unicode
  scalars, valid UTF-8, no control characters and no `:`. Other spellings are
  usage errors (CLI, exit 2) or manifest errors (exit 1).
- `entry` is an optional schema-1 key; older manifests stay valid. The whole
  manifest is decoded, so a malformed `entry` fails even in unselected targets.
- CLI `--entry` overrides the manifest key and requires exactly one selected
  target. It conflicts with check/print actions and `--new-project`; those
  actions ignore manifest `entry` (as they ignore `output`).
- An explicit entry is validated in every compiling mode (default, `--emit`,
  IR/type inspection), together with ordinary semantic diagnostics.

| Failure | Diagnostic (exit 1) |
| --- | --- |
| Module not in the batch | `<origin>: entry module M is not among the compiled modules` (origin: `--entry` or manifest `file:line:col [target t] (entry)`) |
| No `F/1` | `<file>:<line>:<col>: entry function M:F/1 is not defined` at the module declaration |
| Only other arities | `... is not defined; found F/N, but the entry receives one argument (the argument list)` at that definition |
| `F/1` not exported | `<file>:<line>:<col>: entry function M:F/1 is not exported` at the definition |
| No selection, no `main/1` export | `no entry point: no module exports main/1; choose the entry with --entry MODULE[:FUNCTION] (an exported FUNCTION/1; FUNCTION defaults to main)` |
| No selection, several | `ambiguous entry point: main/1 is exported by a, b; choose the entry with ...` (same hint) |

For project targets the hint also names the manifest key: `... --entry MODULE[:FUNCTION] or with
entry = "MODULE[:FUNCTION]" in this target's [[targets]] table of the project manifest ...`, for example:

```toml
[[targets]]
name = "app"
sources = ["src/*.erl"]
entry = "app:start"   # calls app:start/1; plain "app" calls app:main/1
```

## Arguments

`Entry(Argv)` receives a proper list of strings (lists of Unicode code points),
excluding the program name, unchanged and in order, like `escript`.

- POSIX: each argument's bytes are decoded as UTF-8; a byte that does not start
  a valid sequence becomes the code point of that byte (Latin-1 fallback).
- Windows: the CRT's wide (UTF-16) argument vector, split by the same rules as
  `argv`; an unpaired surrogate becomes U+FFFD.
- No option parsing, globbing or environment expansion happens in the runtime.

## Exit status

| Outcome | Status |
| --- | --- |
| Entry returns (any value) | 0 |
| `erlang:halt()` | 0 |
| `erlang:halt(N)`, non-negative integer | `N` (POSIX hosts keep the low 8 bits) |
| `erlang:halt(Slogan)` with a string | Slogan on stderr, then 1 (no crash dump) |
| `erlang:halt(abort)` | Native abort (no flushing) |
| Any exception escaping the entry, including `throw` and `exit(normal)` | Report on stderr, 1 |
| Entry process killed by an exit signal | Report on stderr, 1 |
| Runtime startup or infrastructure failure (ABI mismatch, registration, memory before entry) | Message on stderr, 70 |
| Process stack budget exceeded (body recursion beyond 2^24 stack words) | `erlangaot: runtime failure: entry call failed: resource_limit`, 70 |
| Process heap budget exceeded (live heap words and created off-heap binaries beyond 64 MiB after collection, [heap exhaustion](runtime-heap.md#failure-behavior)) | `erlangaot: runtime failure: entry call failed: resource_limit`, 70 |

Invalid `halt/1` arguments raise `badarg` in the caller. When the entry
finishes, the program exits: other processes are stopped without running
further, as with OTP's `halt/1` after `escript` returns.

`erlang:halt/0,1` is callable with an explicit `erlang:` prefix (unqualified
auto-imported calls and `halt/2` arrive with the builtin bridge, step 36).
`error/1,2,3`, `exit/1` and `throw/1` are callable with or without the prefix;
a local definition or `-compile({no_auto_import, ...})` keeps the unqualified
name local, as in OTP.
`halt(N)` keeps the low 31 bits of any non-negative integer, as OTP does. A
slogan is a proper list of at most 1,023 Unicode code points. A halt unwinds the
entry through the checked error channel like an error, so it stops the program
only after generated cleanup.

## Output streams

- stdout: `standard_io` output (`io:format/1,2`, `io:put_chars/1`,
  `erlang:display/1`). Buffered; flushed on every exit path except `abort`.
- stderr: the uncaught-exception report, `standard_error` output, runtime
  failures and crash reports of other processes.
- The report is one line `uncaught exception <class>: <reason in ~w form>`,
  later followed by stack frames (step 15). Its exact text is not a stable
  interface; tests match it by pattern.

## Startup object

Compiling with an explicit entry (`--entry` or manifest `entry`) adds a startup
module after the batch's modules. With `--emit` it is published
as `eav1_start.{obj,o,ll,bc}` next to the module artifacts (the name cannot
collide with a module artifact). It contains a constant
`abi::v1::StartupDescriptor` ([startup.hpp](../abi/include/erlang_aot/abi/startup.hpp)):
ABI revision, term width, every module descriptor in source order, the entry
module/function spellings and an escript flag. Its `int main(int, char **)`
calls the runtime's `erlang_aot_main_v1`, which:

1. Checks the startup and every module descriptor for ABI revision and width
   before anything is registered; a mismatch exits 70.
2. Starts the runtime and registers all modules; any failure stops before the
   entry and discards the runtime (exit 70), so no Erlang code runs against a
   partial batch.
3. Creates the entry process, builds argv and calls `M:F/1`.
4. Maps the outcome to the exit status above, printing reports after flushing
   stdout, then destroys the process and shuts the runtime down on every path
   (except `halt(abort)`).

`erlangaot -o` links these objects itself ([linking](#linking)). Manual
linking (the [native harness recipe](compile.md#run-the-compiled-module-example)
without a harness source):

```powershell
& $tool --emit obj --entry app --artifact-dir build/app app.erl helper.erl
clang-cl /MT build/app/*.obj build/debug/lib/erlang_runtime.lib /Fe:app.exe
```

Any Clang-compatible link of the objects with `ErlangAoT::generated_program`
works the same way (see `tests/compiler/linking/startup.cmake`).

## Linking

`erlangaot [-O0|-O2|-Os] -o PATH a.erl b.erl ...` (or `--project FILE [--target T] -o PATH`
for one selected target) compiles the batch in memory, adds the startup object
for the [entry](#entry-selection) and links an executable:

```sh
erlangaot -O2 -o build/demo examples/compile/answer.erl examples/compile/client.erl
./build/demo          # build/demo.exe on Windows
```

`-Os` additionally places every generated and runtime function and data object in
its own section and links with `--gc-sections` (ELF), `-dead_strip` (Mach-O) or
`/OPT:REF /OPT:ICF` (MSVC), so code no entry path reaches is removed.

- `PATH` is invocation-relative. For Windows targets, `.exe` is appended when
  the file name has no extension. Its directory must exist.
- Linker: `--linker PATH` (a path or program name), else `clang++` or `clang`
  from `PATH`, then (Windows) `%ProgramFiles%/LLVM/bin`. It runs as
  `<clang> --driver-mode=g++ --target=<triple> -o <staged> <objects> <runtime>`,
  so Clang chooses the platform linker and C/C++ runtime libraries (on Windows it
  locates MSVC and the SDK itself; no developer shell is needed).
- Runtime: `--runtime-library PATH`, else the `erlang_runtime` archive of the
  build that produced `erlangaot` (path recorded relative to the executable, e.g.
  `bin/../lib/erlang_runtime.lib`). Every native object in the archive must match
  the target's architecture and object format; `--target-triple` for another
  target therefore needs a runtime built for it.
- Objects and the executable are staged in a private `.erlangaot-link-*`
  directory beside the output, which is removed afterwards. The output is
  replaced only after a successful link, so every failure keeps an existing
  file unchanged. The output must not be a directory or alias an input.
- Linker warnings are forwarded to stderr; `--linker` and `--runtime-library`
  require `--output` or a linking project build.
- Project builds without `-o` link every selected target that has `output` or
  `entry` to its manifest output, creating missing directories, and replace the
  outputs only after all selected targets linked
  ([projects](projects.md#executables)).

| Failure (exit 1) | Diagnostic |
| --- | --- |
| No Clang | `cannot find clang++ or clang on PATH; install LLVM/Clang or pass --linker` / `linker not found: X` |
| No runtime | `runtime library not found: P; build the erlang_runtime target or pass --runtime-library` |
| Not an archive | `runtime library is not a static library: P: ...` |
| Wrong target | `runtime library P contains x86_64 coff objects, but the executable targets T; ...` |
| Link error | `linking O failed: <clang> exited with status N:` followed by the linker output (first 64 KiB) |
| Bad destination | `output directory does not exist: D`, `artifact destination is not a regular file: O`, `artifact destination aliases an input: O` |

## Escripts

A source file whose first line starts with `#!` is compiled as an escript, in
any mode and with any file name (positional inputs or project `.erl` files).
Rules follow OTP 29 `escript` for source scripts:

- The `#!` line is ignored. An optional comment on line 2 and a `%%!` emulator
  line (line 2, or line 3 after the comment) are comments; `%%!` arguments
  cannot apply to compiled code and produce a warning.
- If the first form is not `-module(...)`, the module is
  `<file name with '.' replaced by '_'>__escript` (`?MODULE` included). OTP adds
  a timestamp/unique suffix; ErlangAoT keeps the name deterministic. The
  synthesized declaration occupies line 1, so later line numbers are unchanged.
- `main/1` is required (`escript does not define main/1`) and implicitly
  exported; other functions follow normal export rules.
- `-mode(compile | interpret | debug | native)` is accepted and ignored; other
  values are errors. Outside escripts `-mode` stays unsupported.
- Entry: without `--entry`, the only escript in the batch is the entry (it wins
  over modules exporting `main/1`); several escripts are ambiguous.
- Exit status as OTP `escript`: an exception escaping the escript entry exits
  127 with `escript: exception <class>: <reason>` on stderr; other rows of the
  exit-status table apply unchanged.
- Files without `#!` are ordinary modules. (OTP `escript file.erl` would skip
  their first line; ErlangAoT does not.) Precompiled beam and archive escripts
  are not supported.

## OTP comparison

Goldens for [program fixtures](../tests/fixtures/programs/README.md) run the
entry under OTP with the same rules
([oracle](../tests/compiler/programs/oracle.escript)); the same oracle generates
the [executable golden cases](../tests/fixtures/executables/README.md) that the
end-to-end runner links and runs under every policy. Differences from
`escript` for ordinary modules: uncaught exceptions exit 1 instead of 127, and
`main/1` must be exported. Escript sources keep OTP's rules (see above).
