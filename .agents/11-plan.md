# Remaining compiler and runtime work — implementation plan 11

Created 2026-10-03 from [completed work](00-finished.md) and
[the feature backlog](01-todo.md). Planning only: every step is unchecked and
creating this plan starts no implementation. It replaces the earlier 51-step
draft with smaller, single-commit steps.

## How to use this plan

- Steps run in numbered order by default. Each lists its backlog owners and
  dependencies; an independent step may move earlier once its dependencies
  pass.
- Each single step ends with one focused commit titled `[plan11] <full step title with step number>`. A step that
  grows beyond one reviewable change is split into lettered sub-steps (`12a`,
  `12b`) before coding, not widened silently.
  - IMPORTANT: Do not add "co-authored by" in commit messages.
- **Decision** steps publish a short contract in `docs/` (plus a prototype where
  stated) and enable no source feature by themselves.
- Check a success or test box only with recorded evidence. Update the backlog
  checkboxes for the slice delivered.
- Optional D-items (steps 71–77) end in a recorded selection, deferral or
  omission. An omitted item is never checked as implemented.
- Phase C (steps 8A–8I) was inserted on 2026-10-04 after phase B closed; its
  steps run before step 9 and keep their letter IDs in commit titles.

## Common gate and rules (apply to every step)

- **Gate:** freshly configure `build/debug` with compiler, runtime and
  `BUILD_TESTING=ON`; build; run fast-mode CTest (`ctest --preset debug-fast`);
  run `cmake --build build/debug --target check-quality` (changed files and
  header dependents). When a phase or major feature completes, run full-mode
  CTest (`ctest --preset debug -j <N>`; each test takes two slots, so N/2 run
  at once) instead of fast mode, never both (full covers every fast run), and
  `check-quality-all`. Lizard and clang-tidy pass
  without raised thresholds or suppressions. Code is clang-formatted; new and
  changed `.erl`/terms files pass erlfmt.
- **Code:** project-internal C++23; document field and function intent in 1–2
  lines; keep cyclomatic complexity low; runtime stays LLVM-free.
- **OTP:** at the start of OTP-dependent work follow
  [otp-reference.md](../docs/otp-reference.md) to check `maint-29`. OTP sources
  and copies never enter git; commit only owned sources, observations and
  golden results. Normal builds and tests require neither OTP nor its checkout;
  goldens change only by explicit, reviewed regeneration.
- **Tests:** prefer real `.erl` sources through the CLI, linked executables and
  OTP-derived goldens. Where applicable cover positional and project drivers,
  O0/O2, specialization on/off and local/remote calls. Add focused unit tests
  only for budgets, ownership, invalid handles and injected faults that source
  cannot reach. Do not remove a useful test before equivalent behavioral
  coverage exists.
- **Representations:** a new term kind needs construction, comparison,
  printing, tracing, copying, destruction and error ownership rules before
  source code may produce it. Forged tagged words are never accepted as valid
  identities.
- **Failures:** keep legality, availability and runtime failure distinct.
  Invalid source is diagnosed even when unreachable; missing services stay
  explicit unavailable-capability diagnostics.
- **Docs:** update affected contracts, examples, `00-finished.md`, `arch.md`,
  `files.md` and `aimemory.md`. Unavailable hosts and tools are recorded as
  gaps, never as passes.

<a id="completed-patternmatch"></a>

## Retained context from the completed pattern/guard plan

Old plan 10 steps 1–20 and added 15a finished on 2026-10-03; their numbers are
historical and unrelated to the steps below. Per-step records are in
[the archive](00-finished.md#completed-patternmatch) and condensed in
[validation history](../docs/validation.md#history).

Executable baseline: ordered function clauses with guards, body
matches/sequences, acyclic local and exported remote calls. Admitted terms:
atoms/booleans, arbitrary integers, finite binary64 floats, tuples,
proper/improper lists, strings, exact-key maps, bitstrings and ordinary tuple
records (declarations, defaults, construction, access, patterns, tests).
Descriptors use ABI revision 4; the checked first-error channel is revision 2.

Invariants that later steps must keep:

- Candidate bindings publish only after head and guard success; `_` binds
  nothing; repeated names use exact equality; body matches evaluate the RHS once
  and chains right-to-left.
- Map keys read only incoming bindings; binary sizes also read preceding
  segments of the same binary.
- Guard legality comes from the pinned catalog and lint rules, not from runtime
  registration. Semantic guard errors reject the alternative; infrastructure
  errors stop execution.
- Every fallible call is checked before its output is used; errors are owned,
  first failure wins, outer cleanup runs, and retry in the same context works.
- Shape proofs dominate extraction; specs never authorize runtime access or
  narrow representation. Specialization limits: 3 variants per function, 32
  per module, 128 per target, 2x generic IR growth, with generic fallback.
- Failed compilation preserves earlier valid outputs; multi-file replacement is
  not atomic.

| Retained contract | Documents |
| --- | --- |
| Patterns, clauses, body matches | [Patterns](../docs/patterns.md) |
| Guards and catalog | [Guards](../docs/guards.md) |
| Bindings, types, inference facts | [Semantic analysis](../docs/semantic.md) |
| Failure channel, atoms, roots, registration | [ABI](../docs/abi.md), [terms](../docs/terms.md#atoms) |
| Representations | [Terms](../docs/terms.md), [runtime memory](../docs/runtime.md#process-memory) |
| Owned fixtures and history | [Validation](../docs/validation.md), [fixture instructions](../tests/fixtures/patternmatch/generated/README.md) |

Last reviewed `maint-29` pin: `21776803ecd11f5fa948732c0ec66b8f325dedfc`;
oracle OTP 29.1.1 / ERTS 17.1. Latest combined Windows x64 Debug gate (phase K close, step 58,
2026-10-09): 224 full-mode CTests; `check-quality-all` at phase J2's close (324 units).

## Step overview

| Phase | Steps | Backlog owners |
| --- | --- | --- |
| A. Baseline and fixtures | [1](#step-1)–[2](#step-2) | V03 |
| B. Production executables | [3](#step-3)–[8](#step-8) | F01, F26, V04 |
| C. Classic process heap | [8A](#step-8a)–[8I](#step-8i) | F02–F05, F09 |
| D. Control flow and exceptions | [9](#step-9)–[16](#step-16) | F13, F14, F16, F20 |
| E. Execution model, recursion, comprehensions | [17](#step-17)–[22](#step-22) | F02, F13, F16, F21, F22 |
| F. Memory management | [23](#step-23)–[28](#step-28) | F02–F05, F08–F11 |
| G. Records, function values, dynamic calls | [29](#step-29)–[35](#step-35) | F03, F12, F14, F17–F19, F21 |
| H. Builtins and libraries | [36](#step-36)–[41](#step-41) | F26, F27 |
| I. Processes and messaging | [42](#step-42)–[53](#step-53), [43A](#step-43a) | F02, F04, F05, F07, F14, F22, F24–F26 |
| J. Multi-worker scheduling | [54](#step-54)–[57](#step-57) | F06, F23, F25, F28 |
| J2. Ports and port I/O | [57A](#step-57a)–[57G3](#step-57g3) | F07, F23, F26, F35 |
| K. End-to-end projects | [58](#step-58) | F01, V03 |
| L. Optimization and tooling | [58A](#step-58a)–[58I](#step-58i), [59](#step-59)–[62](#step-62), [62A](#step-62a), [62B](#step-62b) | F23, F25, F29–F34 |
| M. Validation closure | [63](#step-63)–[70](#step-70) | V01–V04 |
| N. Optional scope decisions | [71](#step-71)–[77](#step-77) | D01–D07 |
| O. Final closure | [78A](#step-78a), [78](#step-78) | all |

---

## Completed steps 1–58A

Each step's full text, criteria and evidence are in Git: the commit named
here, and the plan as it stood there. Contracts in `docs/` hold the current
behavior. Phases A–K are closed; L continues below.

| Step | Title | Commit |
| --- | --- | --- |
| <a id="step-1"></a>1 | Refresh the OTP reference and record a fresh baseline | `2d27503` |
| <a id="step-1a"></a>1A | Tests run time too long | `07d2c38` |
| <a id="step-1b"></a>1B | Quality check checks too much | `3cdc743` |
| <a id="step-1c"></a>1C | Run the available documentation check | `e381a86` |
| <a id="step-2"></a>2 | Author target program fixtures and their feature map | `1a006ac` |
| <a id="step-3"></a>3 | Decide the entrypoint, arguments and exit-status contract | `af55175` |
| <a id="step-3a"></a>3A | Add escript compile mode | `8bb5f89` |
| <a id="step-4"></a>4 | Add runtime term printing and erlang:display/1 | `d63132b` |
| <a id="step-5"></a>5 | Generate the startup object | `6a246ae` |
| <a id="step-6"></a>6 | Link executables from positional CLI inputs | `8f68c18` |
| <a id="step-6a"></a>6A | Link a single project target with explicit -o | `0c27f72` |
| <a id="step-7"></a>7 | Link executables from project targets | `bcaa23f` |
| <a id="step-8"></a>8 | Add the executable golden test runner | `736a194` |
| <a id="step-8a"></a>8A | Decide the classic process heap contract | `02ddf8d` |
| <a id="step-8b"></a>8B | Split binary cells and add the off-heap list | `34005ee` |
| <a id="step-8c"></a>8C | Make heap areas parseable and add a heap walker | `ad0f810` |
| <a id="step-8d"></a>8D | Admit heap words by header instead of the object index | `a0dbc00`, `0c76a8a` |
| <a id="step-8e"></a>8E | Hold host terms as raw words between safe points | `e5aa219` |
| <a id="step-8f"></a>8F | Move generated root frames onto a process stack | `e99e4a8`, `b4c99d3`, `9573722`, `7ac15fb` |
| <a id="step-8g"></a>8G | Replace chunks with a contiguous heap and heap fragments | `c162308` |
| <a id="step-8h"></a>8H | Collect on explicit host request with a copying collector | `d0cf00b` |
| <a id="step-8i"></a>8I | Close the heap rework | `dc6e349` |
| <a id="step-9"></a>9 | Lower `begin`/`end` blocks and `case` expressions | `60102a1` |
| <a id="step-10"></a>10 | Lower `if` expressions | `1982697` |
| <a id="step-11"></a>11 | Raise exceptions from source | `80cd244` |
| <a id="step-12"></a>12 | Lower `catch Expr` | `328a290` |
| <a id="step-13"></a>13 | Lower `try … of … catch` | `27867ed` |
| <a id="step-14"></a>14 | Lower `try … after` | `9df4e63` |
| <a id="step-15"></a>15 | Provide stack traces and `erlang:raise/3` | `c311ae7` |
| <a id="step-16"></a>16 | Lower `maybe` expressions | `aecbff2` |
| <a id="step-17"></a>17 | Decide the frame and continuation model | `5447a16` |
| <a id="step-18"></a>18 | Accept recursive call graphs in analysis | `0e6eb6d` |
| <a id="step-19"></a>19 | Implement proper tail calls | `eff731f` |
| <a id="step-20"></a>20 | Support deep non-tail recursion | `26f8b27` |
| <a id="step-21"></a>21 | Lower list comprehensions | `07d249a` |
| <a id="step-22"></a>22 | Lower binary and map comprehensions | `1f58ba4` |
| <a id="step-23"></a>23 | Extend the root inventory to the execution model | `a3ef043` |
| <a id="step-24"></a>24 | Decide collection triggers and safepoints in generated code | `cf9d82b` |
| <a id="step-25"></a>25 | Collect on explicit runtime request (folded into 8H) | — |
| <a id="step-26"></a>26 | Collect from generated code | `0a94ccf` |
| <a id="step-27"></a>27 | Report heap exhaustion as a defined failure | `bcd1ed5`, `8e939f1`, `3a21bc0` |
| <a id="step-27a"></a>27A | Runtime-wide memory limit and program-facing caps | `6a87dae` |
| <a id="step-27b"></a>27B | Remove list length caps | `5701ab5` |
| <a id="step-27c"></a>27C | Match OTP's tuple arity limit | `12f772d` |
| <a id="step-27d"></a>27D | Remove map size and key-work caps | `d4baa1f` |
| <a id="step-27e"></a>27E | Match OTP's big integer limit | `1c085f9` |
| <a id="step-28"></a>28 | Copy term graphs between heaps | `4d707e5` |
| <a id="step-29"></a>29 | Lower record updates | `ef7e2cc` |
| <a id="step-30"></a>30 | Implement record_info/2 | `7186f18` |
| <a id="step-31"></a>31 | Implement native, qualified and inferred record forms (31A–31E) | — |
| <a id="step-31a"></a>31A | Decide the native record contract | `8e3e18b` |
| <a id="step-31b"></a>31B | Add native record cells and runtime services | `206afc1` |
| <a id="step-31c"></a>31C | Compile local native records | `68252fc` |
| <a id="step-31d"></a>31D | Compile qualified and imported native records | `595650c` |
| <a id="step-31e"></a>31E | Compile anonymous native record forms | `a80d26b` |
| <a id="step-32"></a>32 | Implement function values without captures | `bffcb27` |
| <a id="step-33"></a>33 | Implement closures with captured variables | `76e4713` |
| <a id="step-34"></a>34 | Implement named funs | `4ad9bce` |
| <a id="step-35"></a>35 | Implement dynamic calls | `919f32e` |
| <a id="step-36"></a>36 | Implement the generic production builtin bridge | `78cbb41` |
| <a id="step-37"></a>37 | Add the term-access builtin family | `b29cbfc` |
| <a id="step-38"></a>38 | Add the conversion builtin family | `cd03833` |
| <a id="step-39"></a>39 | Ship a project-owned library subset for lists and maps | `21f1f77` |
| <a id="step-40"></a>40 | Add console output through io | `f6a5c30` |
| <a id="step-41"></a>41 | Add typed native callables for builtin implementations | `09bdac0` |
| <a id="step-42"></a>42 | Implement pid and reference identities | `59064bf` |
| <a id="step-43"></a>43 | Run spawned processes on a cooperative executor | `f092504` |
| <a id="step-43a"></a>43A | Make long-running builtins interruptible | `0682b54` |
| <a id="step-44"></a>44 | Define process exit and crash reports | `98ad97a` |
| <a id="step-45"></a>45 | Implement the signal inbox and message send | `dbad5db` |
| <a id="step-46"></a>46 | Implement selective receive without timeout | `45b424c` |
| <a id="step-47"></a>47 | Implement receive ... after timeouts | `e404460` |
| <a id="step-48"></a>48 | Implement links, exit signals and trap_exit | `82d8187` |
| <a id="step-49"></a>49 | Implement monitors | `e111740` |
| <a id="step-50"></a>50 | Implement registered process names | `d881d4b` |
| <a id="step-51"></a>51 | Collect garbage with mailboxes and suspended processes | `55347de` |
| <a id="step-52"></a>52 | Enable identity-dependent guards | `dd6b8f2` |
| <a id="step-53"></a>53 | Decide port identity scope | `583e779` |
| <a id="step-54"></a>54 | Synchronize the atom table | `25215cf` |
| <a id="step-55"></a>55 | Synchronize code-server publication and lookup | `8b8a699` |
| <a id="step-56"></a>56 | Run processes on multiple scheduler workers | `c1ff627` |
| <a id="step-57"></a>57 | Handle cross-worker wakeups, timers and shutdown | `fa54da5` |
| <a id="step-57a"></a>57A | Decide the port contract | `d39a9e1` |
| <a id="step-57b"></a>57B | Add port identities and the port table | `21a5199` |
| <a id="step-57c"></a>57C | Run the I/O poller with scheduler wakeups | `d1dc2f2` |
| <a id="step-57d"></a>57D | Open subprocesses as ports | `6b04929` |
| <a id="step-57e"></a>57E | Implement file I/O and standard I/O through ports | `b1bc5d7` |
| <a id="step-57f"></a>57F | Implement sockets as ports | `8c99374` |
| <a id="step-57g"></a>57G | Schedule ports like processes on an event-driven backend (57G1–57G3) | — |
| <a id="step-57g1"></a>57G1 | Serve every port kind from one event-driven I/O thread | `fbacc98` |
| <a id="step-57g2"></a>57G2 | Run port tasks on the scheduler workers | `171b7f9` |
| <a id="step-57g3"></a>57G3 | Suspend senders on busy ports and bound unread input | `616e7b2` |
| <a id="step-58"></a>58 | Run the target fixture projects end to end | `3ec5737` |
| <a id="step-58a"></a>58A | Decide the inference fact domain | `e52277c` |

## L. Optimization and tooling (first step)

- <a id="step-58a"></a>**58A. Inference fact domain** — 2026-10-09
  (`docs/semantic.md#inference-domain`, `semantic/types/lattice`). Joins,
  widening and budgets (singletons 8, members 8, depth 4, elements 16); recursive
  results join 8 rounds, then widen with integer thresholds.

## L. Optimization and tooling

Steps 58A–58I (added 2026-10-08, user request; 58H and the entry domains of
58G added 2026-10-09) make type inference precise
enough that `tests/fixtures/inference/values.erl` and `base_types.erl` (every
base and built-in type of the [type language](https://www.erlang.org/doc/system/typespec.html))
reach their `expect:` signatures: today only integer constants, integer joins
and argument relations are inferred (7 of 39 and 3 of 46 functions). Every
built-in type already resolves in declarations. They depend only on the existing
inference (steps 18, 21) and may move earlier. Each step removes the `today:`
lines it closes, adds fixtures for its own cases, and keeps facts sound:
specialization (step 59) may only rely on proven facts, and every widening,
budget or unknown construct still yields `term()`.

<a id="step-58b"></a>

### 58B. Infer facts of all literals

Backlog: F34. Depends on: [58A](#step-58a).

Atoms, floats, characters, strings (`[97 | 98 | 99, ...]`), `[]` and
literal binaries (`<<_:16>>`) get their facts; integer literals already do.

- Success criteria
  - [ ] Every literal and every function returning one infers its fact;
    joins of mixed literals follow the 58A rules (`1 | float()`).
- Tests
  - [ ] `values.erl` literal rows (`float`, `atom`, `string`, `empty_list`,
    `binary`, `integer_or_float`) and `base_types.erl` literal rows (`nil`,
    singleton atoms, `?MODULE`, `<<>>`, `<<_:3>>`, `{}`, `#{}`, `mfa`) reach
    `expect:`; new rows for characters, negative floats and long strings at
    the widening threshold.

<a id="step-58c"></a>

### 58C. Infer operator and builtin results

Backlog: F34. Depends on: [58B](#step-58b).

Arithmetic on known integers folds within the integer limit and otherwise
yields `integer()`, `float()` or `number()` by operand facts (`/` is always
`float()`, `band 255` is `0..255`, `abs/1` of an integer `non_neg_integer()`);
comparisons, `andalso`/`orelse`/`not` and type tests yield `true`, `false` or
`boolean()`. A table gives every bridge builtin its result category: `self/0`,
`spawn/1,3` -> `pid()`, `make_ref/0` -> `reference()`, `length/1`,
`byte_size/1`, `tuple_size/1` -> `non_neg_integer()`, `float/1` ->
`float()`, `trunc/1` -> `integer()`, `list_to_atom/1` -> `atom()`,
`atom_to_list/1` -> `string()`, `integer_to_list/1` -> `nonempty_string()`,
`list_to_binary/1` -> `binary()`, `tuple_to_list/1` -> `list()`,
`list_to_tuple/1` -> `tuple()`, ... Raising paths contribute nothing to a
join, so a function that always raises infers `none()`.

- Success criteria
  - [ ] `sum() -> 3`, `product() -> 42`, `division() -> float()`,
    `comparison() -> true`, `conjunction() -> false`; folding never changes
    runtime behavior (overflow to bignums, badarith stay runtime outcomes).
- Tests
  - [ ] `values.erl` operator rows and the `base_types.erl` builtin, boolean,
    number and `no_return` rows reach `expect:`; new rows for bignum folding,
    `div`/`rem`, mixed integer/float arithmetic and the rest of the builtin
    table.

<a id="step-58d"></a>

### 58D. Infer container facts

Backlog: F34. Depends on: [58B](#step-58b).

Tuples keep element facts; lists join element facts and know nonempty or
empty; strings are lists of character facts; maps keep exact constant keys of
any kind (atoms, integers, tuples, mixed) with value facts, and an update of
an unknown map is `map()`; records keep their tuple shape. Element access
(`element/2`, patterns, `hd/1`) reads element facts back.

- Success criteria
  - [ ] Same-type and mixed-type tuples, lists and maps, nested containers and
    `{ok, 1} | {error, bad}` joins print as their `values.erl` expectations;
    width and depth past the 58A budgets widen to the category.
- Tests
  - [ ] `values.erl` container rows and the `base_types.erl` list, string,
    iolist, improper list, map update, binary construction and comprehension
    rows (`binary()`, `nonempty_binary()`, `nonempty_bitstring()`) reach
    `expect:`; new rows for records, element access, cons cells and
    containers at the budget limits.

<a id="step-58e"></a>

### 58E. Infer fun facts

Backlog: F34. Depends on: [58D](#step-58d), [35](#step-35).

`fun F/A`, `fun M:F/A` and anonymous funs (closures included) get fun facts
with their arity and the callee's or body's result fact; a call of a value
whose fact is a known fun uses that result.

- Success criteria
  - [ ] `returns_fun() -> fun(() -> 42)`, `local_fun() -> fun(() -> 5)`,
    `applies_fun() -> 6`, closures with captured facts; unknown funs and
    `apply/2,3` stay `term()`.
- Tests
  - [ ] `values.erl` fun rows and `base_types.erl` `fun_value`/`remote_fun`
    reach `expect:`; new rows for named funs, funs passed to library functions
    and funs stored in containers.

<a id="step-58f"></a>

### 58F. Infer local function inputs from their callers

Backlog: F34. Depends on: [58B](#step-58b), [18](#step-18).

A function that is neither exported nor referenced by a fun gets the join of
its call sites' argument facts as inputs, iterated with the recursive
components of step 18 (inputs widen like results after the round limit);
exported and fun-referenced functions keep `term()` inputs.

- Success criteria
  - [ ] `increment(3) -> 4` and `call_local() -> 4`; recursive local loops
    converge or widen as results do; specialization profiles stay sound.
- Tests
  - [ ] `values.erl` local rows reach `expect:`; new rows for several call
    sites, recursive locals, a local referenced by `fun f/1` and the widening
    limit.

<a id="step-58g"></a>

### 58G. Narrow facts by patterns and guards; infer entry domains

Backlog: F34. Depends on: [58C](#step-58c), [58D](#step-58d). Entry domains
added 2026-10-09 (user direction).

Inside a clause, a matched pattern and the guard narrow the facts of the
values they test: `is_integer(X), X >= 1, X =< 10` makes `X` the range
`1..10`, `{ok, V}` makes the matched value a two-tuple, `is_float` /
`is_integer` split number joins. Narrowing is the meet of facts
(`Lattice::meet`: the values both facts hold; an empty meet means the clause
or branch can never run). Narrowed facts apply only within the clause (and the
guarded branch of `case`/`if`/`receive`, and the body after a body match).

Type tests narrow their argument (user direction 2026-10-09), wherever a
guard (function clause, `case`, `if`, `receive`, comprehension filter) or a
guard-like condition (the right operand of `andalso` after a true test, the
`true` branch of `case is_integer(X) of true -> ...`) has proved them true:

| Test | Argument narrows to |
| --- | --- |
| `is_atom/1` | `atom()` |
| `is_boolean/1` | `boolean()` |
| `is_integer/1` | `integer()` |
| `is_float/1` | `float()` |
| `is_number/1` | `number()` |
| `is_binary/1` | `binary()` |
| `is_bitstring/1` | `bitstring()` |
| `is_list/1` | `maybe_improper_list()` (proper and improper lists and `[]`; 58G adds this category to the 58A list family) |
| `is_tuple/1` | `tuple()` |
| `is_map/1` | `map()` |
| `is_function/1` | `fun()` |
| `is_function/2` with a literal arity `N` | a fun of arity `N` |
| `is_pid/1`, `is_port/1`, `is_reference/1` | `pid()`, `port()`, `reference()` |
| `is_record/2,3` with a literal name (and size) | a tuple of that size whose first element is the name |
| `is_map_key/2` | its map argument to `map()` |
| The old guard names (`integer/1`, `atom/1`, ...) | as their `is_` forms |

- A test meets the argument's current fact, so a test that contradicts it
  (`is_atom(X)` where `X` is `1..10`) makes the clause or branch impossible.
- A conjunction (`,` and `andalso`) applies every test; a disjunction (`;`,
  `orelse`) narrows to the join of what each alternative proves; `not` and a
  false test prove nothing about the argument's type, except that a clause
  following one whose patterns are all plain variables and whose whole guard
  was a single type test sees the tested variable without that category (the
  earlier clause can only have failed on the test: `scaled/1`'s second clause
  sees a non-integer).
- Comparisons in guards narrow integer facts to ranges (`X >= 1, X =< 10`)
  only once a test or fact made the value an integer; on other values they
  prove nothing, as terms of every type compare.

A function is never entered with arguments no clause's patterns and guards
accept (`function_clause`), so each argument's entry domain is the join over
the clauses of its narrowed fact at clause entry. The domain is the function's
input in `--print-types` (exported functions too: `bounded(1..10) -> 1..10`,
`scaled(number()) -> number()`), the body of each clause starts from it, and a
call that returns narrows the caller's argument variables to the callee's
domain (`f(X), g(X)`: `g` sees `X` within `f`'s domain). A local function's
inputs (58F) meet its domain. Specs never add to a domain.

- Success criteria
  - [ ] `bounded(1..10) -> 1..10`, `scaled(number()) -> number()`; a narrowed
    fact never escapes the clause that proved it, except as the entry domain
    and as the caller's narrowing after a call returns.
- Tests
  - [ ] `values.erl` and `base_types.erl` rows with guards and patterns reach
    `expect:` with their inputs updated to the entry domains
    (`integer_range(1..10)`, `pos_integer_value(pos_integer())`,
    `timeout_value(forever | non_neg_integer())`, ...); new rows for range
    guards, tuple, list and map patterns, a catch-all clause (domain
    `term()`), narrowing after a call returns, and narrowings that must not
    leak; one row per type test of the table (in a function guard, a `case`
    guard and an `andalso` condition), a contradicting test, a disjunction and
    a clause after a single type test; unit tests of `meet` in
    `semantic_inference`.

<a id="step-58h"></a>

### 58H. Narrow facts by their uses

Backlog: F34. Depends on: [58G](#step-58g). Added 2026-10-09 (user
direction).

An operation or call that raises unless an operand has a type proves that
type for every point it dominates on its normal path: after `X + 1` returns,
`X` is `number()` (or the code crashed with `badarith`). Uses narrow
arithmetic operands to `number()` (`div`, `rem`, `band`, `bsl`, ... to
`integer()`), `andalso`/`orelse` left operands and `not` to `boolean()`,
`hd/1`/`tl/1` to a nonempty list, `length/1` to `list()`, `element(N, T)` to
`tuple()` and `pos_integer()`, `tuple_size/1` to `tuple()`, `map_get/2`,
`is_map_key/2` and `M#{K := V}` to `map()`, `atom_to_list/1` to `atom()`,
segments of binary construction to their type (`<<X:8>>`: `integer()`), a fun
call `F(A, B)` to a fun of arity 2, a remote call with a variable module or
name to `atom()`, record field access to the record's tuple, and a call of a
function whose domain is known (58G entry domain, or success domain below) to
that domain. The operand table follows the bridge builtins' argument checks
(`badarg` conditions), one row per builtin.

A function returns normally only for arguments its body's uses accept, so its
success domain per argument is the join over its clauses of the argument's
fact at each normal return; `--print-types` prints it as the input
(`inc(X) -> X + 1` exported: `inc(number()) -> number()`) and callers narrow
their arguments to it after the call returns, as for entry domains.

- Soundness: a narrowing holds only on the normal-completion path after the
  use and never crosses an exception edge (a `catch`, `try` handler or
  `after` sees the facts from before the use); paths narrowed differently join
  at merge points; every name bound to the same value narrows with it; the
  operation keeps its runtime check (specialization, step 59, may drop a check
  only where a proven fact makes it redundant); widened and over-budget facts
  narrow to the use's type, never to more.
- Success criteria
  - [ ] Every use in the table narrows its operand after it on the normal
    path, success domains print as inputs and narrow callers, and no narrowing
    holds on an exception path or before the use.
- Tests
  - [ ] New `values.erl` rows: `increment` exported as
    `increment(number()) -> number()`, `len(L) -> length(L)` as
    `len(list()) -> non_neg_integer()`, `g(X) -> _ = inc(X), X` as
    `g(number()) -> number()`, a use inside `try ... catch` that must not
    narrow after it, a use in one `case` branch only, `hd/1`, `element/2`,
    map updates, fun calls and remote calls with a variable module.

<a id="step-58i"></a>

### 58I. Reject specifications that contradict inferred types

Backlog: F34. Depends on: [58A](#step-58a), [58H](#step-58h). Added
2026-10-08 (user request); renumbered from 58H on 2026-10-09 when 58H (uses)
was inserted.

A `-spec` must not contradict what inference proves: a function's inferred
result must be a subtype of (equal to or narrower than) the union of its
overloads' declared results, a declared argument type that shares no value with
the function's entry or success domain (58G, 58H) contradicts it, and a call
whose inferred arguments fit no overload's declared arguments contradicts the
callee's spec. Today
`types/contracts.cpp` only warns, and only for known integer singletons.
Replace it with a subtype relation over the whole 58A fact domain
(`semantic::types::subtype(declared_graph, declared, inferred_graph,
inferred)`) and make a contradiction a compile error at the `-spec`.

- Rules: unknown facts (`term()`, widened or over budget) never contradict;
  `dynamic()`, `any()` and `term()` admit everything; `none()` /
  `no_return()` admits only a function that never returns, and a function
  that never returns fits any result; type variables and `when` constraints
  are checked through their bounds (an unconstrained variable admits
  everything); opaque and nominal types compare by their own identity outside
  their module and by definition inside it; remote types resolve through the
  batch (unresolved remote types admit everything); `-callback` specs are
  not checked against implementations (behaviours, step 73).
- OTP's compiler does not check specs (Dialyzer does, as warnings): record
  the error in `docs/differences.md`; keep the check sound (no false error is
  acceptable) and report the declared and inferred types in the diagnostic.

- Success criteria
  - [ ] Every inferred result and every call with known arguments that
    contradicts a spec is a compile error naming the function, the declared
    type and the inferred type; a narrower inferred type, an unknown fact and
    every construct in the rules above compile without one.
- Tests
  - [ ] Fixtures (`tests/fixtures/inference/contracts/`) with one
    contradiction per fact kind of 58A (literal, operator result, container,
    fun, range, union, overloads, constraints, opaque/nominal, remote type,
    `no_return()`) each fail with the expected diagnostic; `values.erl`,
    `base_types.erl` and every existing program and fixture compile without
    one (their specs hold); `codegen_types`' deliberate `value() -> 42` vs
    `atom()` case becomes an error test.

<a id="step-59"></a>

### 59. Make specialization remove real source checks

Backlog: F29. Depends on: [58](#step-58), [58G](#step-58g).

- Success criteria
  - [ ] Proven profiles remove tag/shape checks in new operations (arithmetic,
    tuple access, list loops) with generic fallback and existing limits.
- Tests
  - [ ] Same goldens pass with specialization on/off; code size and compile
    time recorded descriptively, not gated.

<a id="step-60"></a>

### 60. Emit source-level debug information

Backlog: F30. Depends on: [58](#step-58).

- Success criteria
  - [ ] Executables carry line tables through macros/includes; a debugger
    breaks on an Erlang line and shows the Erlang call stack.
- Tests
  - [ ] Scripted debugger session (LLDB/GDB where available) on a golden
    program; line-table inspection test runs everywhere.

<a id="step-61"></a>

### 61. Add opt-in profiling

Backlog: F31. Depends on: [58](#step-58).

- Success criteria
  - [ ] An opt-in mode attributes time/reductions per function and per
    process; disabled mode leaves output and artifacts unchanged.
- Tests
  - [ ] Profile a known hot function in a golden program and check it ranks
    first; byte-identical artifacts when disabled.

<a id="step-62"></a>

### 62. Integrate link-time optimization

Backlog: F32. Depends on: [7](#step-7), [58](#step-58).

- Success criteria
  - [ ] An `--lto` option links bitcode with descriptors, exports and startup
    intact on supported toolchains; unsupported targets report it.
- Tests
  - [ ] Fixture goldens pass with LTO; size/build time recorded.

<a id="step-62a"></a>

### 62A. Index code server lookups with hash maps

Backlog: F33. Depends on: [35](#step-35); independent of the other phase L
steps, so it may move earlier. Added 2026-10-07 after step 35.

`CodeServer::export_frame` (dynamic calls, runtime `fun M:F/A`) scans every
registered module and then its export list; `fun_definition`,
`record_definition` and `atom_word` scan modules by descriptor. Replace the
scans with hash maps built at registration: module atom word to its module,
`(function atom, arity)` to the export frame, and descriptor address to its
bindings.

- Success criteria
  - [ ] Dynamic call, apply/3 and runtime `fun M:F/A` lookups take constant
    expected time in the number of modules and exports; descriptor lookups
    likewise.
  - [ ] Maps are built inside the registration transaction (a failed
    registration publishes none of them) and stay valid while modules stay
    registered; when the code server becomes concurrent (phase J), lookups
    stay safe under its synchronization.
- Tests
  - [ ] Existing goldens (`executables_dynamic_calls`, `runtime_funs`) pass
    unchanged; a focused runtime test registers many modules with many
    exports and checks lookups of present, missing and wrong-arity names.
  - [ ] Lookup cost with many modules recorded descriptively, not gated.

<a id="step-62b"></a>

### 62B. Keep receive timers in a timer wheel

Backlog: F23, F25. Depends on: [47](#step-47), [57](#step-57). Added
2026-10-08 during step 47 (user request).

Step 47 keeps one ordered map of deadlines and reads the monotonic clock
before every time slice to find expired receive timeouts. Replace it with a
runtime timer wheel (hashed, hierarchical slots of millisecond ticks, as ERTS
`erl_time_sup`/`erl_hl_timer` do): arming and cancelling a timer is constant
time, the scheduler advances the wheel from a coarse clock reading taken at
most once per tick (not per slice), and only the slots whose time has come
are consulted when a scheduled timer must fire; waiting processes are never
scanned for deadlines.

- Success criteria
  - [ ] Arming, cancelling (a message arrives first) and firing timers cost
    constant expected time per timer; the clock is read at most once per tick
    while processes run, and an idle scheduler sleeps exactly until the next
    occupied slot.
  - [ ] Timeouts never fire early and fire within one tick of their deadline;
    `after 0`, `infinity` and the 0..4294967295 range keep their step-47
    behavior; with multiple workers (phase J) each worker's wheel, or a shared
    one under its synchronization, keeps these guarantees.
- Tests
  - [ ] Existing goldens (`executables_receive_after`,
    `executables_selective_receive`) pass unchanged; a focused runtime test arms
    many timers, cancels most, and checks firing order and that cancelled ones
    never fire; clock readings per slice recorded descriptively, not gated.

## M. Validation closure

<a id="step-63"></a>

### 63. Validate on Linux x86-64

Backlog: V01. Depends on: [58](#step-58).

- Success criteria
  - [ ] Fresh build, full gate and executable goldens pass; versions and counts
    published in `docs/validation.md`.
- Tests
  - [ ] Full gate plus fixture projects at O0/O2.

<a id="step-64"></a>

### 64. Validate 32-bit x86 (Windows x86 and Linux x86)

Backlog: V01. Depends on: [63](#step-63).

- Success criteria
  - [ ] Same as step 63 with 32-bit word width; 28-bit small-integer boundaries
    exercised natively.
- Tests
  - [ ] Full gate plus integer-boundary and fixture goldens.

<a id="step-65"></a>

### 65. Validate Linux AArch64 and 32-bit ARM

Backlog: V01. Depends on: [63](#step-63).

- Success criteria
  - [ ] Same as step 63 on each architecture, or the missing runner recorded
    as a gap.
- Tests
  - [ ] Full gate plus fixture goldens per architecture.

<a id="step-66"></a>

### 66. Validate macOS Apple Silicon

Backlog: V01. Depends on: [63](#step-63).

- Success criteria
  - [ ] Same as step 63 on macOS arm64.
- Tests
  - [ ] Full gate plus fixture goldens.

<a id="step-67"></a>

### 67. Run compiler and frontend sanitizers

Backlog: V02. Depends on: [63](#step-63).

Use a host/SDK combination without the recorded MSVC annotation and allocator
conflicts (Linux is the likely choice).

- Success criteria
  - [ ] Full compiler+runtime ASan, UBSan and LeakSanitizer runs pass; findings
    are fixed, not suppressed; instrumentation scope is documented.
- Tests
  - [ ] Full CTest under each sanitizer configuration.

<a id="step-68"></a>

### 68. Run ThreadSanitizer on the multi-worker runtime

Backlog: V02. Depends on: [57](#step-57), [67](#step-67).

- Success criteria
  - [ ] Process, scheduler and code-server stress tests report no races.
- Tests
  - [ ] Step-56/57 stress tests under TSan, repeated.

<a id="step-69"></a>

### 69. Broaden OTP compatibility evidence

Backlog: V03. Depends on: [58](#step-58).

- Success criteria
  - [ ] Selected applicable upstream Common Test suites run against a matching
    built OTP, kept separate from Clause differential comparisons.
  - [ ] Differential goldens cover every feature enabled by this plan;
    exclusions are published.
- Tests
  - [ ] Opt-in upstream run with recorded commands; normal CTest stays OTP-free.

<a id="step-70"></a>

### 70. Close the test migration ledger

Backlog: V04. Depends on: [58](#step-58).

- Success criteria
  - [ ] Every remaining adapter or synthetic success test either has equivalent
    executable coverage and is removed, or has a recorded reason to stay.
- Tests
  - [ ] Full gate after removals; ledger dispositions cite the replacing tests.

## N. Optional scope decisions

Each step records a decision in its contract document. If an item is selected,
write its own small implementation plan before coding.

<a id="step-71"></a>

### 71. Decide dynamic modules and code upgrades

Backlog: D01. Depends on: [55](#step-55). **Decision.**

- Success criteria
  - [ ] Static-only, native dynamic libraries, or an upgrade model chosen;
    static-only failures for `code:load_*` documented if omitted.
- Tests
  - [ ] Golden check of the documented boundary behavior.

<a id="step-72"></a>

### 72. Decide atom collection

Backlog: D02. Depends on: [54](#step-54). **Decision.**

- Success criteria
  - [ ] Bounded permanent atoms kept, or collection roots and policy defined.
- Tests
  - [ ] Atom-limit golden for the chosen behavior.

<a id="step-73"></a>

### 73. Decide behavior-changing attributes and transforms

Backlog: D03. Depends on: [58](#step-58). **Decision.**

- Success criteria
  - [ ] Per-attribute decision (`compile` options, `parse_transform`,
    `on_load`, others); rejected ones keep diagnostics.
- Tests
  - [ ] CLI diagnostics for each rejected attribute.

<a id="step-74"></a>

### 74. Decide public stage interchange

Backlog: D04. Depends on: [58](#step-58). **Decision.**

- Success criteria
  - [ ] A concrete consumer named, or deferral recorded; inspection dumps stay
    non-stable.
- Tests
  - [ ] None beyond the gate unless selected.

<a id="step-75"></a>

### 75. Confirm intermediate-stage reader reservations

Backlog: D05. Depends on: [74](#step-74). **Decision.**

- Success criteria
  - [ ] Directory reservations remain the only artifact per AGENTS.md, unless
    a D04 consumer requires a reader.
- Tests
  - [ ] None beyond the gate.

<a id="step-76"></a>

### 76. Decide C/FFI interoperability

Backlog: D06. Depends on: [58](#step-58). **Decision.**

- Success criteria
  - [ ] An external caller and minimal API named, or deferral recorded.
- Tests
  - [ ] None beyond the gate unless selected.

<a id="step-77"></a>

### 77. Decide project schema extensions

Backlog: D07. Depends on: [58](#step-58). **Decision.**

- Success criteria
  - [ ] Demand recorded for each extension (profiles, dependencies, exclusions,
    packages, watch/cache, parallel builds); selected items get their own plan.
- Tests
  - [ ] None beyond the gate unless selected.

## O. Final closure

<a id="step-78a"></a>

### 78A. Reset every versioned ABI name to v1

Backlog: all. Depends on: steps 1–70 and any selected optional work. Inserted
2026-10-05.

The project has never been released, so no earlier generated-code contract has
to stay loadable. Collapse every version marker to v1: runtime service symbols
(`CLAUSE_roots_enter_v5`, `CLAUSE_roots_leave_v4`,
`CLAUSE_raise_v2`, `CLAUSE_call_failed_v2`,
`CLAUSE_register_module_v4`, `CLAUSE_atom_v3`,
`CLAUSE_exception_v2`, `CLAUSE_reraise_v2` and the rest), C++
namespaces such as `abi::v1`/`v2`, `abi::v1::version` and descriptor revision
numbers, the generated `clausev1_` prefixes if any other revision exists, and the
revision tables in the docs.

- Success criteria
  - [ ] No `_v2`-or-later suffix, versioned namespace or revision number above
    1 remains in sources, generated IR, tests, fixtures or docs; the ABI
    document describes one revision 1 without a revision history.
  - [ ] `runtime_symbols.hpp` aliases mirror the renamed `abi/include`
    declarations; no mangled spelling is hardcoded elsewhere.
- Tests
  - [ ] `tests/compiler/codegen/mangling.cpp` spellings, cross-target import
    checks, native consumers and every test calling a service directly use the
    v1 names, checked against Clang for every target ABI and width.
  - [ ] Fresh full gate and `check-quality-all` pass.

<a id="step-78"></a>

### 78. Publish the final implementation and validation boundary

Backlog: all. Depends on: steps 1–70, [78A](#step-78a) and any selected optional work.

- Success criteria
  - [ ] `00-finished.md`, `01-todo.md`, `arch.md`, `files.md`, contracts and
    examples reflect exactly what is implemented, validated, deferred or
    omitted.
  - [ ] README shows building and running a multi-process Erlang program as an
    executable.
- Tests
  - [ ] Fresh full gate on every available host; README commands executed as
    written.
