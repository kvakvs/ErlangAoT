# Function values

Plan 11 steps 32–34 (2026-10-07): `fun F/A`, `fun M:F/A`, anonymous funs
with captured variables (closures), named funs (`fun Name(...) -> ... end`)
and calls of function values (`F(Args)`). Facts come from the pinned `maint-29` sources
(`erts/emulator/beam/utils.c` `erts_cmp`, `erl_printf_term.c`, `erl_lint`)
and probes on OTP 29.1.1.

## Values

| Source | Value | Calls enter |
| --- | --- | --- |
| `fun f/1` | Local fun of this module; every `fun f/1` of a module is the same value | `f/1` of this module, exported or not |
| `fun m:f/1` | External fun naming `m:f/1`, also for the current module | `m:f/1` when a module of the program exports it |
| `fun(X) -> ... end` | Local fun capturing the variables it reads from its creator; each evaluation builds a new value | The fun's own generated function |
| `fun Name(X) -> ... end` | The same, with `Name` bound to the fun inside its clauses | The fun's own generated function |

- `fun F/A` must name a function of the module (`function F/A undefined`).
  Naming an auto-imported builtin (`fun is_atom/1`), `fun erlang:F/A` and
  `fun M:F/A` with variables report the unavailable `dynamic calls`
  capability (plan step 35): builtins are not callable as values yet.
- `F(Args)` evaluates `F`, then the arguments left to right, then checks the
  value: a non-function raises `{badfun, F}`, another arity
  `{badarity, {F, Args}}` (checked before the module), an external fun whose
  function nothing in the program exports `undef`. A call in tail position is
  a tail call, as for named functions.
- `is_function/1,2` are true for funs in bodies and guards.

## Closures

- Each clause starts from the scope at the fun. Head variables are new names
  that shadow outer ones (OTP warns); a name repeated within one head must
  match. Guards read the head's names. Nothing a fun binds is visible after
  it, and case-clause names of the enclosing function do not reach into it.
- A fun captures every outer definition its heads, guards or bodies read
  (nested funs included), in definition order, as OTP orders a function's
  free variables. Captured values are copied into the fun cell when the fun
  expression is evaluated, so they survive the creator's return and
  collections, and copy with the fun.
- The code is a private function `-f/A-fun-N-` (N counts the funs of `f/A` in
  source order) taking the fun's arguments, then its captured values; stack
  traces show that name with the combined arity, as OTP's do. No clause
  matching raises `function_clause`.
- Arguments plus captured values are limited to 255.
- A named fun's clauses see `Name` as the fun itself: a new name shadowing an
  outer one (OTP warns), never captured and not visible after the fun. A head
  variable of the same name shadows it in turn. When a clause reads `Name`, the
  fun's code builds the value on entry from its captured values, so it is equal
  (`=:=`) to the fun being called, and `Name(...)` is an ordinary fun call:
  in tail position it is a tail call and runs in constant stack.
- An anonymous fun in a record field default is one fun for every
  construction that uses the default (OTP expands a copy per site).

## Representation

- **Descriptor.** Each distinct value a module creates compiles to one
  `abi::v1::FunDescriptor` in the module's private `<prefix>.funs` table
  ([funs.hpp](../abi/include/erlang_aot/abi/funs.hpp)): module descriptor,
  module and function atom slots, Erlang arity, index, external flag and the
  `FrameDescriptor` a call enters (null for an external fun outside the
  program). Registration binds them to `FunDefinition`s
  (`ModuleAtoms::funs`, `CodeServer::fun_definition`); a local fun's captured
  value count is its code's arity minus the fun's.
- **Cell.** Boxed kind `fun_closure`: header (count `1 + n`), an untraced
  `const FunDefinition *`, then `n` captured values. Walking,
  collection, copying and verification skip the definition word, as for
  native records.
- **Services.** `erlang_aot_make_fun_v1(context, descriptor, captures, count,
  output)` builds a fun. `erlang_aot_apply_v1(context, fun, arity, arguments)`
  checks a called value, records the errors above in the failure channel
  (`ErrorReason` 22-24) and otherwise appends the captured values after the
  arguments and returns the `FrameDescriptor` to enter.
- **Calls.** Native form passes the arguments in a word array and calls the
  `erlang_aot.apply` marker with the descriptor; `lower_frames` makes the
  array the process registers and the marker a transfer through
  `erlang_aot_enter_v1` or, in tail position, `erlang_aot_tail_v1`.

## Comparison and printing

- Term order: number < atom < fun < tuple < native record < map < nil < list
  < bitstring (references, ports and pids, which sit between atoms and tuples
  in OTP, do not exist yet).
- Local funs order before external funs. Local funs compare by module, then
  index, then captured values in order (`==` compares those with `==`);
  external funs by module, function and arity. Equal descriptors give equal
  values, so `fun f/1 =:= fun f/1`.
- `erlang:display/1`, `~w` and uncaught-exception reports print an external
  fun as `fun m:f/1` (atoms quoted like the emulator does) and a local fun as
  `#Fun<m.Index.0>`.

## Differences

Recorded in [differences](differences.md):

- A local fun prints `#Fun<m.Index.0>`: OTP's index follows its compiler's
  lambda numbering and its third part is a hash of the module code.
  ErlangAoT numbers local funs in source order, so the order of two local funs
  of different functions of a module can also differ.
- Calling an external fun of a module outside the program raises `undef`; OTP
  would first try to load the module from the code path.
- Anonymous fun names (`-f/1-fun-0-`, seen in stack traces) count funs in source
  order; OTP's compiler numbers them in its own order. Funs created inside
  comprehensions may also capture their values in another order than OTP's,
  which only shows when two such funs are compared.
