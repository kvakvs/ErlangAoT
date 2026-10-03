# Executables

Contract for programs built by `erlangaot -o`. Entry selection is validated
today; startup and linking arrive with plan 11 steps 5–7, so `-o` still ends
with `[executable linking] notimpl` after a valid entry is found.

## Entry selection

The entry is an exported function of arity 1 that receives the argument list.

| Source | Spelling | Scope |
| --- | --- | --- |
| CLI | `--entry MODULE[:FUNCTION]` | Positional batch, or the single selected project target |
| Manifest | `entry = "MODULE[:FUNCTION]"` in a `[[targets]]` table | That target |
| Default | Only escript, else only module exporting `main/1` | Only when an executable is requested |

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
| No selection, no `main/1` export | `no entry point: no module exports main/1; select one with --entry or the manifest entry key` |
| No selection, several | `ambiguous entry point: main/1 is exported by a, b; select one ...` |

## Arguments

`Entry(Argv)` receives a proper list of strings (lists of Unicode code points),
excluding the program name, unchanged and in order, like `escript`.

- POSIX: each argument's bytes are decoded as UTF-8; a byte that does not start
  a valid sequence becomes the code point of that byte (Latin-1 fallback).
- Windows: wide (UTF-16) arguments; an unpaired surrogate becomes U+FFFD.
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

Invalid `halt/1` arguments raise `badarg` in the caller. When the entry
finishes, the program exits: other processes are stopped without running
further, as with OTP's `halt/1` after `escript` returns.

## Output streams

- stdout: `standard_io` output (`io:format/1,2`, `io:put_chars/1`,
  `erlang:display/1`). Buffered; flushed on every exit path except `abort`.
- stderr: the uncaught-exception report, `standard_error` output, runtime
  failures and crash reports of other processes.
- The report is one line `uncaught exception <class>: <reason in ~w form>`,
  later followed by stack frames (step 15). Its exact text is not a stable
  interface; tests match it by pattern.

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
([oracle](../tests/compiler/programs/oracle.escript)). Differences from
`escript` for ordinary modules: uncaught exceptions exit 1 instead of 127, and
`main/1` must be exported. Escript sources keep OTP's rules (see above).
