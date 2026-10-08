# Semantic analysis

Runs after parsing in default compilation and `--print-types`; syntax-only
actions skip it. Errors stop the affected batch before LLVM; diagnostics keep
macro/include origins, and later inputs are still diagnosed.

## Module and call checks

- Module declaration required and unique; function arities 0..255; no duplicate
  definitions; every export exists and is listed once. Quoted/Unicode names keep
  their exact identity.
- Every function is checked, including unused and unreachable code.
- Calls resolve within the batch by module/name/arity. Remote calls (including
  self-qualified) require exports. Missing/private callees and duplicate
  modules are errors.
- `fun F/A` must name a function of the module (`function F/A undefined`);
  `fun M:F/A` and calls of values (`F(Args)`) are not resolved at compile time
  ([funs](funs.md)). Each distinct fun value gets a `Module::funs` entry after
  binding analysis.
- Self, mutual and cross-module recursion are accepted. The call graph is split
  into strongly connected components (iterative Tarjan) in callee-before-caller
  order; a component is recursive when it has several members or a member
  calls itself.

## Bindings

Each binding has a function-relative identity `clause[N].local[M]`. Occurrences
are definitions, reads or exact-equality checks, tagged with head/guard/body
context. Analysis is deterministic.

- Each clause starts from an empty environment. Head candidates collect
  tentative definitions; guards read them; the body sees them only on success.
- `_` binds nothing; `_Name` is an ordinary variable. A repeated name is an
  exact-equality constraint.
- Body matches visit RHS before LHS; chains are right-to-left. A parenthesized
  pattern `P1 = P2` is an alias, not a sequence.
- Sibling expressions read the same incoming names; their definitions export
  after the whole expression. `{X = 1, X}` is an unbound read; `{X = 4, X = 3}`
  is legal and fails at run time.
- Definitions on the RHS of `andalso`/`orelse` or inside `catch Expr` are unsafe
  afterwards (OTP `vtunsafe`); names bound before a `catch` stay usable. Every
  name bound inside a `try` is unsafe afterwards; `of` clauses see the body's
  names, catch clauses see them as unsafe, and the after body sees every name
  bound earlier in the try as unsafe. A catch clause's stack variable must be
  new (OTP `stacktrace_bound`) and its guard must not read it
  (`stacktrace_guard`). A `maybe` exports nothing: each `?=` binds for the
  following body expressions, `else` clauses see the body's names as unsafe
  and every name bound inside is unsafe afterwards. Guards never publish
  bindings; matches in guards are errors even when unreachable.
- Map keys read only incoming bindings; binary sizes also read earlier segments
  of the same binary (see [patterns](patterns.md#scopes)).
- A `case` scrutinee binds in the enclosing scope. Each clause starts from that
  scope; its pattern definitions are tentative until the guard, which only
  reads. Names bound by every clause are exported with one identity (later
  clauses reuse the identity an earlier clause gave the name); names bound by
  only some clauses, or unsafe in any, are unsafe afterwards. Exports join
  conservatively like OTP's `erl_lint` (`icrt_export`); OTP's warning when a
  later pattern matches an exported name is not emitted. `if` clauses follow
  the same rules with a guard and no pattern.
- `begin`/`end` is a sequence in the enclosing scope.
- Unbound, unsafe and wildcard reads are located errors. Messages keep the
  compiler's wording (`unbound variable X`, `unsafe variable X`); the bindings
  corpus checks each against OTP's `unbound_var`/`unsafe_var` class.
- A comprehension evaluates each qualifier in order in the scope left by the
  previous ones. Generator patterns bind new names that shadow outer ones
  (reads inside the pattern prefer names it already bound); a zip group binds
  all its patterns together. Templates read like siblings. The scope after the
  comprehension is the one before it: its names are unbound there.
- An anonymous fun's clauses each start from the scope at the fun: head
  names are new and shadow outer ones, guards read them, and nothing bound
  inside is visible after the fun. Every outer definition read inside is
  recorded as a capture (`Function::captures`, definition order). A named
  fun's name is one more definition every clause starts with
  (`Function::fun_names`); it is never captured. The variables of
  `fun M:F/A` are reads of the fun expression (`Function::fun_operands`).

Walks are iterative with a module budget of 1,000,000 work units. Exhaustion or
any semantic error clears the module's binding and normalization tables.

## Types and specifications

A private type graph represents all parsed type forms independently of runtime
layout: singletons, ranges, containers, map field roles, function products and
unresolved applications. Unions flatten and deduplicate; `term()` is top and
`none()` bottom. Defaults: 16,384 nodes, 16 union members, 100,000 work items per
translation. Exhaustion widens to `term()` with a visible flag and never narrows
a representation.

Declared metadata (`-type`, `-opaque`, `-nominal`, `-export_type`, `-spec`,
`-callback`, `-optional_callbacks`) follows the OTP typespec reference and
pinned `erl_lint`/`erl_internal`/`erl_types` behavior:

- Local aliases may shadow builtin type names. Remote batch types must be
  exported. Missing types, duplicates, malformed metadata, singleton type
  variables, invalid bounds and specs for missing functions are errors.
  Unavailable external types warn and become `term()`.
- Each alias or overload has its own variable scope; repeated formals follow
  OTP's last-argument substitution.
- Recursive aliases stay finite named references; opaque bodies expand only in
  their module; nominal names are kept everywhere.
- Constant evaluation is exact, limited to 10,000 decimal digits per value.

## Inference

Inference is separate from declared types and never trusts specs.

- Exported inputs are arbitrary terms. Integer literals keep singleton values;
  identity/projection functions keep exact argument relations, propagated
  through nested local and remote calls with fresh variables per call.
- Whole-value body assignments and aliases copy the RHS fact
  (`Y = 42, Z = Y, id(Z)` infers 42). Extracted fields, guard refinements,
  service results and unproved values stay `term()` without relations.
- Clause results join conservatively: a projection survives only if every
  clause returns the same argument. A `case` or `if` joins its clause results
  the same way; a binding defined by several of its clauses stays `term()`.
- A recursive component starts every member's result at `none()` and re-infers
  all members until no result changes; each round joins the new result with the
  previous one, and a pending recursive call adds nothing to a join. A function
  that can never return stays `none()`. After 16 rounds without convergence
  every member widens to `term()` (reported as widened, like budget
  exhaustion) and a final round recomputes the expression
  facts; earlier rounds' expression facts are discarded, so only facts from the
  final assumptions remain.
- A shared work budget bounds inference; exhaustion loses precision and falls
  back to generic code, never rejects a program.
- Specs are checked only for provable contradictions with known integer
  results/arguments, producing warnings. This is not success typing. Plan
  step 58H makes any contradiction between a `-spec` and the inferred types an
  error (inferred must be the declared type or narrower).

Lowering consumes these facts. Generated IR never converts an integer to a heap
pointer; each fallible service result is loaded only on its success path, and
shape checks dominate extraction. Specialization policy:
[specialization.md](specialization.md).

## `--print-types`

Prints each module of the batch (input/target order, library modules after
them) as Erlang source ([source printing](compile.md#source-printing)) with what
type inference found. Output is on stdout and is human-readable, not Erlang and
not an interchange format. Warnings stay on stderr.

```erlang
%% module "branches" source="branches.erl" target="" declared=complete inferred=complete
-module(branches).
-export([mixed/1]).

%% inferred: mixed(term()) -> 1 | 2
mixed(X) ->
    case X of
        1 ->
            1;
        _ ->
            2
    end :: 1 | 2.
```

- A `%% module` line names the module, its source, the project target and
  whether declared and inferred types completed or were widened by a limit.
- Declarations (`-type`, `-spec`, `-callback`, records) appear as written.
- Above each function, `%% declared: f(Inputs) -> Result` gives each overload
  of its `-spec` as resolved (with its `when` constraints), and
  `%% inferred: f(Inputs) -> Result` what inference found, so the two can be
  compared. Inputs of exported functions and of functions with
  specifications stay `term()`. A `-spec` stays with the function right after
  it, set apart from other forms by a blank line.
- Expressions whose fact says more than `term()` are annotated
  `Expression :: Type`: in parentheses inside other expressions, without them
  for a whole body expression. Literals and matches are not annotated (the
  right side of a match is).
- `argument N` means the value is the function's N-th argument (1-based), as
  inference proved; a variable shows only its type, its name already says
  which argument it is.

### Inference expectations

`tests/fixtures/inference/*.erl` record what inference should find for each
function, and what it finds today. Each module becomes the CTest
`inference_<module>` (`tests/compiler/inference/expectations.py`):

```erlang
%% expect: sum() -> 3
%% today: sum() -> term()
sum() -> 1 + 2.
```

- `expect:` is the signature `--print-types` should print in its
  `%% inferred:` line; `today:`, present while inference falls short, is the
  one it prints now.
- The check compares the output with `today` when there is one, else with
  `expect`; every function of the module needs an `expect` line. A `today`
  line that inference has caught up with fails the check until it is removed.
- `expectations.py <erlangaot> <fixture> --record` rewrites the `today`
  lines from the current output (a maintainer action: review the diff).
- `values.erl` covers literals, arithmetic and comparisons, calls of local and
  other functions, integer joins and ranges, integers or floats, lists,
  strings, tuples, maps with atom and other keys, funs returned and applied,
  binaries and argument relations. Today inference finds integer constants,
  integer joins and argument relations (7 of 39 functions).
- `base_types.erl` has a function per base and built-in type of the
  [type language](https://www.erlang.org/doc/system/typespec.html) (`pid()`,
  `reference()`, bitstrings and binaries, ranges, `byte()`, `char()`,
  `non_neg_integer()`, `boolean()`, `string()`, `iolist()`, `mfa()`,
  `timeout()`, `no_return()`, ...): its `-spec` names the type, so every
  built-in type is checked to resolve, and its body produces such a value.
  Categories are expected under their built-in names, bounded integer sets as
  ranges. Today 3 of 46 functions reach their expected type.

### Printing types

`semantic::types::type_source(graph, type)` (`semantic/types/printing`) renders
a type of the type graph in Erlang type syntax: `term()`, `none()`, atoms and
integers, `1..5`, `{ok, T}`, `tuple()`, `[T]`, `[T, ...]`, `#{K => V, K := V}`,
`#r{f :: T}`, `<<_:B, _:_*U>>`, `fun((A) -> R)`, `A | B`. Predefined `erlang`
types drop their module; references to declared types stay named. A budget of
nodes bounds the text; past it, and below 32 levels of nesting, `...` stands in.

```sh
erlangaot --print-types answer.erl client.erl
erlangaot --print-types --project project.toml --target demo --verbose
```
