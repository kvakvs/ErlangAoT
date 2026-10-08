# Target program fixtures

Small, original Erlang programs that represent the end goal: whole projects that
build into executables and behave like OTP. Each directory holds:

| File | Meaning |
| --- | --- |
| `project.toml` | Schema-1 manifest with one target named after the fixture |
| `src/*.erl` | Locally authored modules (no OTP source) |
| `fixture.json` | Entry module and argv strings |
| `expected/stdout.txt`, `expected/golden.json` | OTP-generated stdout and exit status, with input, generator and oracle hashes |
| `compile.txt` | Today's `clau --project project.toml` exit status and exact stderr (paths shown as `<fixture>`) |

The goldens assume the proposed [step-3](../../../.agents/11-plan.md#step-3)
contract: `Entry:main(Argv)` receives a list of strings; normal return exits 0;
`erlang:halt/1` sets the status; an uncaught exception exits 1. Stderr is not
compared. Only `main` prints, and timing-dependent paths wait for explicit
messages, so output is deterministic.

## Tests

- `programs_compile` (normal CTest, OTP-free) verifies every golden hash, then
  compiles each fixture and compares exit status and stderr with `compile.txt`.
  When a plan step enables a feature, rerun
  `python tests/compiler/programs/programs.py <clau> build/programs --update-diagnostics`
  and review the `compile.txt` changes with that step.
- Regeneration is an explicit maintainer action using an installed OTP 29;
  `--check` writes nothing and fails on drift (also CTest `programs_oracle`
  with `-DCLAUSE_OTP_AUDITS=ON`):

```powershell
python tests/compiler/programs/regenerate.py --escript 'C:/Program Files/Erlang OTP/erts-17.1/bin/escript.exe' --check
```

Never regenerate to make a Clause comparison pass. Step 58 will run these
goldens against linked executables with the
[step-8 runner](../executables/README.md).

## Feature map

Every fixture needs the executable path: entry contract, term printing, startup,
project linking and the golden runner (steps [3](../../../.agents/11-plan.md#step-3)–[8](../../../.agents/11-plan.md#step-8)),
the frame model ([17](../../../.agents/11-plan.md#step-17)), recursion and tail
calls ([18](../../../.agents/11-plan.md#step-18), [19](../../../.agents/11-plan.md#step-19)),
the builtin bridge ([36](../../../.agents/11-plan.md#step-36)), the `lists`/`maps`
subset ([39](../../../.agents/11-plan.md#step-39)), `io:format` with `~w ~s ~b ~n`
([40](../../../.agents/11-plan.md#step-40)) and the end-to-end run
([58](../../../.agents/11-plan.md#step-58)). Additional steps:

| Fixture | Program | Language features | Builtins and library | Additional plan steps |
| --- | --- | --- | --- | --- |
| `textstats` | Text/number CLI over argv; exits with the invalid-argument count (2) | `case`, `if`, `try … of … catch error:badarg`, list comprehensions, map generator, map update, local and anonymous funs, `++`, bignum product | `halt/1`, `list_to_integer/1`, `integer_to_list/2`, `float_to_list/2`, `length/1`, `map_size/1`, `min/2`, `max/2`; `lists:reverse/foldl/sort`, `maps:find` | 9, 10, 11, 13, 21, 22, 32, 33, 37, 38 |
| `frames` | Binary protocol encoder/decoder with corrupted, truncated and malformed streams | Bit syntax construction/matching, binary and bitstring comprehensions, sub-byte bitstrings, `/utf8` segments, `throw` with `try … of … catch`, macros, body recursion | `iolist_to_binary/1`, `list_to_binary/1`, `binary_to_list/1`, `atom_to_list/1`, `list_to_atom/1`, `byte_size/1`, `bit_size/1`, `length/1`; `lists:reverse` | 4 (bitstring printing), 9, 11, 13, 21, 22, 37, 38 |
| `avltree` | Persistent AVL tree library over tuple records, random and 1,000 sorted keys | Record construction, update and patterns, `record_info/2`, `if`, `case`, `throw`/`try`, `orelse`, external and local fun values, closures, named fun, fun-variable calls, `apply/3` | `hd/1`, `abs/1`, `max/2`; `lists:foldl/nth/seq/reverse` | 9, 10, 11, 13, 21, 26, 29, 30, 32, 33, 34, 35, 37 |
| `ring` | 1,000-process ring passing a token for 12 rounds | `spawn_monitor/1` closures, send, selective receive, `'DOWN'` collection, list comprehensions | `self/0`, `list_to_integer/1`, `length/1`; `lists:seq/filter` | 21, 28, 33, 38, 42, 43, 44, 45, 46, 49, 51, 54–57 |
| `kvstore` | Registered key-value server with a call protocol | Server loop, `make_ref/0` tagged replies, selective receive, `after` 0/finite timeouts, map patterns and comprehension, `try … catch error:badarg` on send to a dead name, monitors | `register/2`, `whereis/1`, `monitor/2`, `map_size/1`; `lists:sort`, `maps:find/keys` | 9, 11, 13, 22, 28, 33, 37, 42, 43, 45, 46, 47, 49, 50 |
| `supervise` | One-for-one supervisor with a restart limit, plus link/monitor checks | `trap_exit`, `'EXIT'` messages, `exit/1,2` including `kill`, name release on exit, map comprehension, receive inside comprehensions, `after infinity` | `spawn/3`, `spawn_link/1,3`, `process_flag/2`, `register/2`, `whereis/1`, `monitor/2`, `demonitor/2` (`flush`, `info`), `is_map_key/2`, `map_get/2`; `lists:sort`, `maps:from_list/keys/values` | 11, 21, 22, 28, 33, 37, 42, 43, 44, 45, 46, 47, 48, 49, 50 |

The list operators `++`/`--` (rejected today as `[arithmetic] notimpl`) are
owned by step [37](../../../.agents/11-plan.md#step-37).
