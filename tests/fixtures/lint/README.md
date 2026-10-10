# Lint diagnostic cases

Compile-time errors and warnings compared with OTP's compiler
([plan 11 step 65A](../../../.agents/11-plan.md#step-65a)). Each directory is
one case: Erlang sources and `golden.json`. CTest `lint_diagnostics`
(`tests/compiler/lint/lint.py`) compiles every case's files as one `clau`
batch and compares its exit status and diagnostics (file, line, column,
severity, message; in any order) with the golden.

| `golden.json` field | Written by | Meaning |
| --- | --- | --- |
| `schema` | author | Always 1 |
| `files` | author | The case's sources, in the order OTP compiles them; each compiled module is loaded, so a behaviour module comes before its users |
| `exit_status`, `diagnostics` | OTP | 1 when any file has an error; every error and warning OTP reports |
| `oracle_version`, `reference`, `inputs` | OTP | Oracle release, `maint-29` pin, and source hashes that make a stale golden fail |

Cases cover only diagnostics whose wording Clause shares with OTP; other
lint messages differ ([differences](../../../docs/differences.md)) and stay
in `tests/compiler/semantic/cases.cmake`. A case must not draw OTP warnings
Clause does not emit, such as unused functions.

To add a case, write the sources and a golden with `schema` and `files`,
erlfmt the sources, then generate it once (OTP is needed only here):

```powershell
python tests/compiler/lint/regenerate.py --escript 'C:/Program Files/Erlang OTP/erts-17.1/bin/escript.exe' --case NAME
python tests/compiler/lint/regenerate.py --escript '...' --check   # writes nothing; also CTest lint_oracle
```

Never regenerate to make a Clause comparison pass.
