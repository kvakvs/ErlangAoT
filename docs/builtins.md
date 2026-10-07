# Builtins

Plan 11 step 36 (2026-10-07): the production builtin bridge. Builtins are
`erlang` functions the runtime implements in C++. The runtime registers them
by module, function and arity; generated code reaches them directly, through
dynamic calls and as fun values.

## Which builtins exist

The bridge catalog `abi::v1::bridge_builtins`
([builtins.hpp](../abi/include/erlang_aot/abi/builtins.hpp)) lists every
builtin both the compiler and the runtime know:

- the guard BIFs: type tests (`is_atom/1` … `is_tuple/1`, `is_function/1,2`),
  `abs/1`, `bit_size/1`, `byte_size/1`, `ceil/1`, `element/2`, `float/1`,
  `floor/1`, `hd/1`, `length/1`, `map_get/2`, `map_size/1`, `is_map_key/2`,
  `max/2`, `min/2`, `round/1`, `size/1`, `tl/1`, `trunc/1`, `tuple_size/1`,
  `binary_part/2,3`;
- the operators as functions: comparisons, `not/1`, `and/2`, `or/2`,
  `xor/2`, arithmetic and bitwise operators (`erlang:'+'/1,2` …);
- `display/1`, `halt/0,1`, `error/1,2,3`, `exit/1`, `throw/1`, `raise/3` and
  `function_exported/3`;
- term access (plan step 37): `setelement/3`, `make_tuple/2,3`,
  `tuple_to_list/1`, `list_to_tuple/1` and the list operators `'++'/2` and
  `'--'/2` (`A ++ B`, `A -- B` lower to them).

Entries are only appended: an entry's index is the number generated code
passes to the bridge service. Other `erlang` functions keep their diagnostics:
a direct call of an unknown one is `unknown module erlang`, `fun erlang:F/A` or
`fun F/A` of a guard BIF outside the catalog (`self/0`, `node/0`) and
`fun erlang:apply/2,3` report the unavailable `dynamic calls` capability.

## How calls reach them

| Source | Path |
| --- | --- |
| `abs(X)`, `X + Y`, `erlang:display(X)`, `halt()`, `error(R)` | Inline services, as before the bridge |
| `erlang:function_exported(M, F, A)`, `setelement(I, T, V)`, `A ++ B`, `A -- B` (catalog builtins without an inline service) | `erlang_aot_builtin_v1(context, index, arguments, output)` |
| `fun abs/1`, `fun erlang:'+'/2` | External fun `erlang:F/A`; registration binds it to the builtin |
| `M:F(Args)`, `apply(M, F, Args)`, runtime `fun M:F/A` | The code server finds a module's export first, then a builtin |

- `fun F/A` of an auto-imported builtin the module neither defines nor
  suppresses (`-compile({no_auto_import, ...})`) is the external fun
  `erlang:F/A`, as in OTP: `fun abs/1 =:= fun erlang:abs/1` and it prints as
  `fun erlang:abs/1`. `halt/0,1`, `setelement/3`, `tuple_to_list/1` and
  `list_to_tuple/1` are auto-imported like OTP's; `display/1`, `raise/3`,
  `function_exported/3` and `make_tuple/2,3` need the `erlang:` prefix.
- A builtin has a `FrameDescriptor` with a null body (`BuiltinFrame`). Entering
  it (`erlang_aot_enter_v1`, `erlang_aot_tail_v1`) pushes no frame: the
  builtin runs on the registers and its result returns into the caller's
  body, so a builtin in tail position returns to the caller's caller.
- Errors are the ones the inline lowering raises in a body: `badarg`,
  `badarith` for arithmetic, `system_limit`, `{badmap, M}`, `{badkey, K}`;
  `raise/3` with an invalid class or stack returns `badarg`. They go through
  the checked failure channel like every service error.
- Term access follows OTP's `badarg` rules: `setelement/3` needs a small
  integer index within the tuple; `make_tuple/2,3` a small size in
  0..16,777,215, and `make_tuple/3` a proper list of `{Index, Value}` pairs
  within the size (later pairs win); `list_to_tuple/1` a proper list;
  `A ++ B` a proper list `A` (`[] ++ B` is `B` for any `B`, a non-list `B`
  ends the result); `A -- B` two proper lists, each element of `B` removing
  the first exactly equal (`=:=`) element of `A`, in O((n + m) log m).
- Every builtin runs to completion; work that grows with the input becomes
  interruptible once the scheduler exists (plan step 43A).
- `function_exported(M, F, A)` raises `badarg` unless `M` and `F` are atoms
  and `A` a small integer; it is true when a module of the program exports
  `M:F/A` or `M:F/A` is a registered builtin.

## Registration

- `BuiltinRegistry` ([builtin_registry.hpp](../runtime/include/erlang_aot/runtime/builtin_registry.hpp)),
  owned by the `CodeServer`, maps exact module/function/arity to a
  `BuiltinFrame`. `add` takes a batch of `BuiltinEntry`s and registers all or
  none: an empty name, more than 255 arguments, a missing implementation, or a
  name already registered (or repeated in the batch) rejects the batch with
  nothing kept.
- Runtime startup registers every table of `production_builtins()`
  (`erlang_builtins()`, `term_access_builtins()`), which together cover the
  catalog; later families add their own tables.
- A body reads exactly its arity of argument words and records errors in the
  checked channel; host exceptions become `out_of_memory` or `internal_error`
  failures.
- The older host path `abi::v1::dispatch_builtin` (host-registered native
  modules by name, [runtime](runtime.md#code-server-and-builtins)) is separate
  and unchanged.
