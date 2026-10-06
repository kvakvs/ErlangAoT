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
| Map printing (`erlang:display/1`, `~w`) | Internal layout order (atom-table order for atom keys, hash order above 32 keys) | Map-key order, as OTP's `~kw` | [terms](terms.md#printing) |

## Errors, stack traces and reports

| Difference | OTP | ErlangAoT | Owner |
| --- | --- | --- | --- |
| Stack trace locations | `[{file, F}, {line, L}]`, `error_info` for `error/3` | Always `[]`; no `error_info` | [ABI](abi.md#stack-traces) |
| `function_clause` top frame | Argument list | Arity | [ABI](abi.md#stack-traces) |
| Failing BIF/operator frames | Present (`{erlang, '+', Args, ...}`) | Absent; nothing below the entry function | [ABI](abi.md#stack-traces) |
| Calls to functions that never return | Compiled as tail calls (caller missing from the trace) | Ordinary calls (caller present) | [ABI](abi.md#stack-traces) |
| Big integer limit | `system_limit` | `resource_limit` above 1,000,000 bits | [terms](terms.md) |
| Unbounded body recursion | Process grows until memory runs out | Stops at the 2^24-word process stack: exit 70, `resource_limit` | [executables](executables.md#exit-status) |
| Live data beyond 64 MiB per process | No default limit (`max_heap_size` 0): the process grows until memory runs out | Allocation fails once heap, fragments and created off-heap binaries exceed 64 MiB after collection: exit 70, `resource_limit` | [runtime heap](runtime-heap.md#failure-behavior) |
| Bitstring size limit | Limited by memory (`system_limit` far above) | `resource_limit` when constructing a value above 1,000,000 bits | [terms](terms.md#bitstrings) |
| Uncaught exception in an ordinary entry module | `escript` exits 127 | Exits 1 with one `uncaught exception <class>: <reason>` line (escript sources keep 127) | [executables](executables.md) |
| Compiler diagnostics | `erl_lint` wording (`variable 'X' is unbound`) and warnings | Own wording (`unbound variable X`); OTP lint warnings are mostly not emitted | [semantic](semantic.md#bindings) |

## Language edge cases

| Difference | OTP | ErlangAoT | Owner |
| --- | --- | --- | --- |
| Zip group whose relaxed and strict generators share a variable | Skip test keeps strict-pattern variables in the relaxed patterns | A rejected step is skipped whenever the strict patterns match on their own | [patterns](patterns.md#comprehensions) |
| Files without `#!` given as escripts | `escript file.erl` skips the first line | Compiled as ordinary modules | [executables](executables.md) |
| Precompiled beam and archive escripts | Run | Not supported | [executables](executables.md) |

