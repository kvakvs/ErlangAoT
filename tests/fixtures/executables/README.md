# Executable golden cases

End-to-end cases for the executable golden runner
([plan 11 step 8](../../../.agents/11-plan.md#step-8)): Erlang sources are
linked into a program by `erlangaot`, run with arguments, and stdout, exit
status and a stderr pattern are compared with an OTP-generated golden.

## A case

A directory holding the Erlang sources (`*.erl`, `*.hrl`) and `golden.json`.
Every such directory becomes the CTest `executables_<directory>`; nothing else
is registered.

| `golden.json` field | Written by | Meaning |
| --- | --- | --- |
| `schema` | author | Always 1 |
| `entry` | author | Entry module; `main/1` receives argv strings ([contract](../../../docs/executables.md)) |
| `sources` | author, optional | Case-relative source paths instead of the directory's `.erl`/`.hrl` files (the `demo` case uses `examples/compile/`) |
| `runs[].args` | author | Argument strings of one invocation |
| `runs[].stderr` | author, or `^$` | Python regular expression searched in stderr; required when OTP writes stderr, because ErlangAoT's report text differs |
| `runs[].exit_status`, `runs[].stdout` | OTP | Observed under the pinned OTP with the program-fixture [oracle](../../compiler/programs/oracle.escript) |
| `oracle_version`, `reference`, `inputs` | OTP | Oracle release, `maint-29` pin, and source hashes that make a stale golden fail before linking |

To add a case, write the sources and a golden with the author fields, then
generate it once (OTP is needed only here; erlfmt the sources first because the
hashes cover them):

```powershell
python tests/compiler/executables/regenerate.py --escript 'C:/Program Files/Erlang OTP/erts-17.1/bin/escript.exe' --case NAME
python tests/compiler/executables/regenerate.py --escript '...' --check   # writes nothing; also CTest executables_oracle
```

Never regenerate to make an ErlangAoT comparison pass.

## Runner

`tests/compiler/executables/run.py <erlangaot> <work> <case> --suffix=<.exe>`
stages the sources, links them under each combination of
`tests/compiler/patternmatch/matrix.py` (full mode: O0/O2 × specialization
on/off × positional `--entry … -o` and project manifest `entry`/`output`; fast
mode: O0 positional and O2-off project), runs every `runs[]` entry and prints a
unified stdout diff, exit-status and stderr-pattern mismatches.
`executables_selfcheck` proves a wrong golden fails with that diff and a stale
golden fails before linking. The tests need the runtime library and Clang, as
`linking_executable` does.
