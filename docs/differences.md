# Differences from Erlang/OTP

Observable behavior where ErlangAoT knowingly differs from the pinned OTP 29
(`maint-29`, see [otp-reference.md](otp-reference.md)). Each entry links to the
contract that owns it. Remove an entry once the difference is fixed. Features
not implemented yet (reported as `notimpl`) are not listed here; see
[features.md](features.md) and the plan.

## Maps

| Difference | OTP | ErlangAoT | Owner |
| --- | --- | --- | --- |
| Map generator order (`K := V <- M`) | Flat maps (up to 32 keys) iterate in key order, but atom keys in atom-table order, which varies between VM runs; larger maps in hash order | Always canonical key order (term order) | [patterns](patterns.md#comprehensions) |
| `bad_generators` payload of a map generator in a zip group | Its iterator: `{K, V, Next}` chain ending in `none`, in OTP's order | The same chain, built in canonical key order | [patterns](patterns.md#comprehensions) |
| Map printing (`erlang:display/1`, `~w`, `~p`) | Internal layout order (atom-table order for atom keys, hash order above 32 keys) | Map-key order, as OTP's `~kw` | [terms](terms.md#printing) |
| Native record printing (`erlang:display/1`) | Fields in atom-table index order | Definition order, as `~w` | [native records](native-records.md#printing-and-order) |
| `==` between native records whose fields differ only as integer/float | The compiler may fold it to `=:=` (false) | Numeric comparison (true) | [native records](native-records.md#printing-and-order) |
| Local fun printing (`#Fun<M.Index.Uniq>`) | Index from the compiler's lambda numbering, `Uniq` a hash of the module code | Index of the module's local funs in source order, `Uniq` always 0 | [funs](funs.md#comparison-and-printing) |
| Order of two local funs of one module | By OTP's index | By source-order index, so funs of different functions can order differently | [funs](funs.md#comparison-and-printing) |
| Anonymous fun names in stack traces (`-f/1-fun-N-`) | `N` from the compiler's numbering | `N` counts the funs of `f/1` in source order | [funs](funs.md#closures) |
| An anonymous fun in a record field default | Each construction site expands its own copy: funs from two sites are unequal | One fun for the default: funs from any construction site are equal | [funs](funs.md#closures) |
| Captured value order of funs created inside comprehensions | Free variables of the comprehension's generated function | Definition order in the enclosing function; visible only when comparing two such funs | [funs](funs.md#closures) |

## Pids and references

| Difference | OTP | ErlangAoT | Owner |
| --- | --- | --- | --- |
| Pid numbers | The first user process is about `<0.80.0>`; numbers are reused after the pid table wraps | The first process is `<0.1.0>`; numbers come from one sequence and are never reused | [terms](terms.md#pids-and-references) |
| Reference numbers | Mix a scheduler identifier and per-scheduler counters (`#Ref<0.178111994.4235460610.214105>`) | One program-wide counter (`#Ref<0.0.0.1>`), so references order by creation | [terms](terms.md#pids-and-references) |

## Errors, stack traces and reports

| Difference | OTP | ErlangAoT | Owner |
| --- | --- | --- | --- |
| Stack trace locations | `[{file, F}, {line, L}]`, `error_info` for `error/3` | Always `[]`; no `error_info` | [ABI](abi.md#stack-traces) |
| `function_clause` top frame | Argument list | Arity | [ABI](abi.md#stack-traces) |
| `undef` top frame | `{M, F, Args, []}` of the missing function | The calling function's frame | [funs](funs.md#dynamic-calls) |
| Failing BIF/operator frames | Present (`{erlang, '+', Args, ...}`) | Absent; nothing below the entry function | [ABI](abi.md#stack-traces) |
| Atom table full (`list_to_atom/1`) | The emulator aborts (`no more index entries in atom_tab`) and writes a crash dump | `erlangaot: runtime failure: entry call failed: resource_limit`, exit 70 | [builtins](builtins.md#how-calls-reach-them) |
| `list_to_integer/1,2` with characters above 255 | Its first digits use only each character's low byte (`[16#131]` is 1) | `badarg` | [builtins](builtins.md#how-calls-reach-them) |
| `erlang:function_exported/3` of a BIF | True for every BIF of the emulator | True only for the builtins this runtime provides | [builtins](builtins.md#how-calls-reach-them) |
| Calls to functions that never return | Compiled as tail calls (caller missing from the trace) | Ordinary calls (caller present) | [ABI](abi.md#stack-traces) |
| Host memory exhausted | The emulator reports that it cannot allocate memory, writes a crash dump and stops | `erlangaot: runtime failure: entry call failed: out_of_memory`, exit 70, no dump | [runtime heap](runtime-heap.md#failure-behavior) |
| Process memory cap | `max_heap_size` (in words, heap and stack) kills the process with reason `killed` and logs an error report | `--max-heap` / `--max-stack` (bytes) and the runtime-wide `--max-memory` fail the requesting process as `resource_limit`: `erlangaot: runtime failure: entry call failed: resource_limit`, exit 70 | [runtime heap](runtime-heap.md#runtime-memory-limit) |
| Program arguments | `escript` passes every argument to `main/1`; emulator flags (`+t`) come from `%%!` or `ERL_FLAGS` | Leading runtime options (`--max-atoms`, `--max-heap`, `--max-stack`, `--max-memory`, `--args-file`, `--`) are taken out first; `ERLANG_AOT_FLAGS` holds the same options | [executables](executables.md#runtime-options) |
| Uncaught exception in an ordinary entry module | `escript` exits 127 | Exits 1 with one `uncaught exception <class>: <reason>` line (escript sources keep 127) | [executables](executables.md) |
| Compiler diagnostics | `erl_lint` wording (`variable 'X' is unbound`) and warnings | Own wording (`unbound variable X`); OTP lint warnings are mostly not emitted (also unknown native record fields in access, update and patterns) | [semantic](semantic.md#bindings) |

## Language edge cases

| Difference | OTP | ErlangAoT | Owner |
| --- | --- | --- | --- |
| Integer segment wider than the integer limit, value past it (`<<V:4194241>>` of all ones) | The x86 JIT matches with an invalid term; using it crashes the VM | No match | [terms](terms.md#integers) |
| Zip group whose relaxed and strict generators share a variable | Skip test keeps strict-pattern variables in the relaxed patterns | A rejected step is skipped whenever the strict patterns match on their own | [patterns](patterns.md#comprehensions) |
| Calling an external fun, `M:F(Args)` or `apply/3` of a module outside the program | Loads the module from the code path, `undef` when absent | `undef`; library modules join the program only when it names them with a literal atom | [funs](funs.md#dynamic-calls), [library](library.md#how-programs-get-them) |
| `lists` and `maps` | The full modules; `maps:keys/1`, `values/1`, `to_list/1`, `fold/3` follow the map's internal order (atom-table order for atom keys, hash order above 32 keys) | The [library subset](library.md); map functions follow key order | [library](library.md) |
| Files without `#!` given as escripts | `escript file.erl` skips the first line | Compiled as ordinary modules | [executables](executables.md) |
| Precompiled beam and archive escripts | Run | Not supported | [executables](executables.md) |

## io

| Difference | OTP | ErlangAoT | Owner |
| --- | --- | --- | --- |
| Control sequences `~e ~f ~g ~x ~X ~+ ~# ~W ~P`, modifier `K` | Formatted | `badarg` | [io](io.md#formats) |
| `~p` of containers nested more than 256 deep | Printed | `system_limit` | [io](io.md#pretty-printing-p) |
| Field widths and `~ts` precision with combining characters or `\r\n` | Count grapheme clusters | Count code points | [io](io.md#formats) |
| Negative precision or pad count (`~.*c` with -1) | Loops forever | `badarg` | [io](io.md#formats) |
| `io` functions other than `format/1,2`, `put_chars/1` | Exist | `unknown module io` | [io](io.md#calls) |

