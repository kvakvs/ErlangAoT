# Library modules

Plan 11 step 39 (2026-10-07): a project-owned subset of OTP's `lists` and
`maps` modules, written in Erlang from OTP's documented behavior (not copied
from OTP sources) and compiled with the programs that use it.

## Contents

| Module | Functions |
| --- | --- |
| [`lists`](../library/stdlib/lists.erl) | `append/1,2`, `filter/2`, `foldl/3`, `foldr/3`, `keyfind/3`, `map/2`, `member/2`, `nth/2`, `reverse/1,2`, `seq/2,3`, `sort/1` |
| [`maps`](../library/stdlib/maps.erl) | `find/2`, `fold/3`, `from_list/1`, `get/2`, `keys/1`, `put/3`, `to_list/1`, `values/1` |
| [`os`](../library/stdlib/os.erl) | `cmd/1` over a port ([subprocesses](ports.md#subprocesses)); `type/0` and `getenv/1` are runtime builtins |

Results and error reasons match OTP 29, including the error shapes of OTP's
Erlang implementations (`lists:map(F, x)` is `{case_clause, x}`,
`lists:nth(0, L)` `function_clause`, `lists:seq(1, 10, 0)` `badarg`) and of
its BIFs (`lists:member/2`, `lists:keyfind/3`, `lists:reverse/2`, the `maps`
functions: `badarg`, `{badmap, M}`, `{badkey, K}`). `lists:member/2` matches
with `=:=`, `lists:keyfind/3` with `==`. `lists:sort/1` is a stable merge
sort. Stack traces name the library functions, not OTP's.

## How programs get them

- When a batch (positional inputs or one project target) names a module with
  a literal atom — `M:F(...)` with literal `M`, `fun M:F/A`, or
  `apply(M, F, Args)` with literal `M` — that no input defines, and
  `<library>/M.erl` exists, the compiler parses and compiles that file into
  the batch, then repeats for the modules the added files name.
- `<library>` is `library/stdlib` of the build tree, found relative to
  `erlangaot` like the runtime archive.
- A module of the batch with the same name replaces the library module.
- Library modules are ordinary batch modules: they link into executables and,
  for object or IR output, produce their own artifacts.
- A module reached only through runtime names (`M:F(...)` with `M` a
  variable) is not added: such a call raises `undef` unless something else
  names the module.
- Library functions are Erlang code, so they will yield like other code once
  the scheduler exists.
