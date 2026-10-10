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
oracle OTP 29.1.1 / ERTS 17.1. Latest combined Windows x64 Debug gate (steps 63-65, 2026-10-09):
235 full-mode CTests; `check-quality-all` clean (338 units, headers now checked); Linux x86-64 235/235.

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
| L. Optimization and tooling | [58A](#step-58a)–[58N3](#step-58n3), [59](#step-59)–[62B](#step-62b) | F23, F25, F29–F34 |
| M. Validation closure | [63](#step-63)–[70](#step-70), [65A](#step-65a), [65B](#step-65b), [65C](#step-65c) | V01–V04, D03 |
| N. Optional scope decisions | [71](#step-71)–[77](#step-77) | D01–D07 |
| O. Final closure | [78A](#step-78a), [78B](#step-78b), [78](#step-78) | all |

---

## Completed steps

Each step's full text, criteria and evidence are in Git: the commit named
here, and the plan as it stood there. Contracts in `docs/` hold the current
behavior. Phases A–L are closed; phase M continues with steps 66, 69
and 70 below.

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
| <a id="step-58b"></a>58B | Infer facts of all literals | `a6c01b5` |
| <a id="step-58c"></a>58C | Infer operator and builtin results | `e92650f` |
| <a id="step-58d"></a>58D | Infer container facts | `6adc311` |
| <a id="step-58e"></a>58E | Infer fun facts | `ebac255` |
| <a id="step-58f"></a>58F | Infer local function inputs from their callers | `8b1e998` |
| <a id="step-58f1"></a>58F1 | Print consecutive integers as ranges | `bfab7a8` |
| <a id="step-58g"></a>58G | Narrow facts by patterns and guards; infer entry domains | `22dff64` |
| <a id="step-58h"></a>58H | Narrow facts by their uses | `c56d515` |
| <a id="step-58h1"></a>58H1 | Narrow integer ranges by guard comparisons | `16fb58e` |
| <a id="step-58i"></a>58I | Reject specifications that contradict inferred types | `be76267` |
| <a id="step-58i1"></a>58I1 | Print term() as `_` | `579e605` |
| <a id="step-58i2"></a>58I2 | Print argument relations as variable names | `6d02bd1` |
| <a id="step-58j"></a>58J | Keep wide known containers and print records | `bc94017` |
| <a id="step-58j1"></a>58J1 | Infer try and maybe values | `2607172` |
| <a id="step-58k"></a>58K | Keep per-clause function types and print them | `c7f52b6` |
| <a id="step-58l"></a>58L | Match calls against function types | `01fbba5` |
| <a id="step-58m"></a>58M | Re-analyse callees for each call | `06042fa` |
| <a id="step-58n1"></a>58N1 | Keep dependent facts of case and if | `e63e14c` |
| <a id="step-58n2"></a>58N2 | Carry dependent facts through operations and bindings | `60a5dd4` |
| <a id="step-58n3"></a>58N3 | Dependent facts of tuple scrutinees and try ... of | `175b545` |
| <a id="step-59"></a>59 | Make specialization remove real source checks | `8a5ace2` |
| <a id="step-60"></a>60 | Emit source-level debug information | `116cff1` |
| <a id="step-61"></a>61 | Add opt-in profiling | `ecb8de5` |
| <a id="step-62"></a>62 | Integrate link-time optimization | `5ec3645` |
| <a id="step-62a"></a>62A | Index code server lookups with hash maps | `aa0198b` |
| <a id="step-62b"></a>62B | Keep receive timers in a timer wheel | `eccd7c4` |
| <a id="step-63"></a>63 | Validate on Linux x86-64 | `5948a44` |
| <a id="step-64"></a>64 | Validate 32-bit x86 (Windows x86 and Linux x86) | `9eef9cc` |
| <a id="step-65"></a>65 | Validate Linux AArch64 and 32-bit ARM | `ab5943b` |
| <a id="step-67"></a>67 | Run compiler and frontend sanitizers | `58b3243` |
| <a id="step-68"></a>68 | Run ThreadSanitizer on the multi-worker runtime | `9679aea` |

### Notes kept from completed steps

- Inference 58A–58N3 (user direction 2026-10-08/10): contract
  [semantic analysis](../docs/semantic.md#inference), domain budgets in
  [inference domain](../docs/semantic.md#inference-domain), dependent facts
  of `case`/`if`/`try ... of` in
  [dependent facts](../docs/semantic.md#dependent-facts) (printed as
  `$case_of_operator`, `$if_operator`, `$try_of_operator`). Expectations
  `tests/fixtures/inference/` (values 143, base_types 46, narrowing 53,
  clauses 53, dependent 25 functions, 13 contract fixtures). Facts stay
  sound: specialization reads only proven facts and the erased union
  summaries. Not done: the optional 58L report of a call no inferred
  function type can enter; a `case` on a variable that itself depends on an
  argument does not split over that argument.
- 59: proofs at O2 with specialization only; source variants still rarely
  fit the 2x size limit.
- 60: Mach-O needs `dsymutil` (not run; step 66, docs/debugging.md).
- 62: `--lto` on Windows MSVC and ELF targets only.
- 62B: "within one tick" is bounded by the host's timed-wait resolution.
- 63: clang-tidy runs on the Windows host only (user); `cmake/tidy_filter.py`
  ignores Boost `cpp_int` analyzer reports (user decision).
- 64: no 32-bit LLVM SDK, so `clau` stays x64 and 32-bit coverage is the
  runtime plus cross-linked goldens (`cross.py`).
- 65: ARM ran under qemu-user only; qemu's `posix_spawn` never reports a
  missing program (`port_spawn` `enoent` fails there). Native ARM is open.
- 67/68: ASan/UBSan/LSan and TSan on Linux x86-64 found and fixed one
  shutdown use-after-free and one lock-order inversion; injected
  host-refusal tests and allocator-replacing tests are out of scope.

## M. Validation closure

<a id="step-65a"></a>

### 65A. Implement `-behaviour`/`-behavior` attributes

Backlog: D03 (selected slice). Depends on: [58](#step-58). Added 2026-10-10
(user direction); runs before every other remaining step.

Accept both spellings instead of rejecting them as behavior-changing
attributes, and check the module's exports against the declared callback API
as described in
[OTP design principles: behaviours](https://www.erlang.org/doc/system/design_principles.html#behaviours)
and pinned `erl_lint` (`check_behaviour`).

- Success criteria
  - [x] `-behaviour(M)` and `-behavior(M)` are accepted and repeatable.
    `module_info/0,1` does not exist in Clause yet, so the attributes are not
    visible at run time (decided 2026-10-10; [65B](#step-65b) adds it).
  - [x] Callbacks of `M` come from its `-callback` and `-optional_callbacks`
    declarations, resolved from batch modules and the shipped library; a
    module declaring `-callback` exports a generated `behaviour_info/1`
    (`callbacks`, `optional_callbacks`), as OTP's `erl_internal` adds it, and
    like OTP its own source cannot export, call, `fun` or `-spec` it.
  - [x] Each required callback missing from the exports reports OTP's
    `undefined callback function F/A (behaviour 'M')` warning; optional
    callbacks are exempt. Also `behaviour M undefined`,
    `conflicting behaviours`, OTP's module-name errors and the error for
    `-callback` beside a hand-written `behaviour_info/1`, in OTP's wording and
    location, honoring `-compile` `nowarn_*` options. A hand-written
    `behaviour_info/1` is not evaluated, so ill-defined and deprecated
    callback warnings cannot occur; `warnings_as_errors` stays a rejected
    compile option (step 73).
  - [x] An unresolvable OTP behaviour (`gen_server`, `supervisor`,
    `application`, ...) follows OTP's `undefined_behaviour` warning path, not
    an error; divergences are recorded in `docs/differences.md`.
  - [x] Contract recorded in `docs/semantic.md`; step 73 lists `behaviour`
    as decided.
- Tests
  - [x] CLI fixtures: a project behaviour module with required and optional
    callbacks, a conforming implementer, missing/conflicting/undefined cases;
    diagnostics compared with OTP-derived goldens.
  - [x] Executable golden calling the implementer through
    `M:behaviour_info(callbacks)` and a dynamic callback call.
- Evidence (2026-10-10): `lint_diagnostics` (8 cases, OTP 29.1.1 goldens),
  `executables_behaviours`, semantic CLI cases; fresh fast CTest 234/234,
  check-quality clean ([validation](../docs/validation.md#history)).

<a id="step-65b"></a>

### 65B. Generate `module_info/0,1` and keep informational attributes

Backlog: D03 (selected slice). Depends on: [65A](#step-65a). Added 2026-10-10
(user direction); runs right after 65A, before every other remaining step.

Every module gets OTP's predefined, exported `module_info/0` and
`module_info/1` ([modules](https://www.erlang.org/doc/system/modules.html),
`erl_internal:add_predefined_functions`: both call
`erlang:get_module_info/1,2`), and informational attributes stop being
rejected. Behavior-changing attributes (`on_load`, `nifs`, `-compile` options
other than `nowarn_*`/`no_auto_import`, parse transforms, wider `-import`)
stay rejected for [73](#step-73).

Observed under OTP 29.1.1 (2026-10-10), to be confirmed by goldens:
`module_info()` is `[{module,M},{exports,_},{attributes,_},{compile,_},{md5,_}]`;
`exports` lists the source exports in order, then `behaviour_info/1` when
generated, then `module_info/0`, `module_info/1`; `attributes` keeps every
attribute except `module`, `export`, `import`, `export_type`, `type`,
`opaque`, `nominal`, `spec`, `callback`, `optional_callbacks`, `record`,
`compile`, `file`, `doc`, `moduledoc`, in source order, a non-list value
wrapped in a list (`{behaviour,[shape]}`, `{my_attr,[again]}`), and adds
`{vsn,[Integer]}` (from the MD5) when the source has no `-vsn`;
`module_info(bogus)` raises `badarg`.

- Success criteria
  - [x] `module_info/0,1` are generated and exported like the 65A
    `behaviour_info/1`, but, unlike it and as in OTP's erl_lint, local calls
    and `fun module_info/1` reach them, an explicit export only warns
    (`function module_info/0 already exported`), and a hand-written
    definition is an error (`function module_info/0 already defined`).
  - [x] `module_info/1` answers `module`, `exports`, `attributes`, `compile`,
    `md5`, `functions`, `nifs` (`[]`) and `native` (`false`). Decided
    2026-10-10: the generated functions return literal data, so no runtime
    or ABI change is needed and `erlang:get_module_info/1,2` is not provided
    (recorded in `docs/differences.md`).
  - [x] Accepted informational attributes: `vsn`, `author`, `copyright`,
    `deprecated`, `behaviour`/`behavior`, `dialyzer` and any other
    user-defined literal attribute (`-my_attr(Term).`), all visible through
    `module_info(attributes)`; names reserved for behavior changes keep the
    capability diagnostic.
  - [x] Values that cannot match OTP (`md5` bytes, the `vsn` integer
    derived from it, `compile` `version`/`options`/`source`, `functions`
    entries OTP adds for its own generated code) have a documented Clause
    meaning and rows in `docs/differences.md`; contract in `docs/semantic.md`
    (or a new `docs/modules.md`), backlog and step 73 updated. Generated
    functions get no debug locations, source comments or `--print-types`
    output.
- Tests
  - [x] Executable golden: `module_info/0,1` keys, `exports`, `attributes`
    with `-vsn`, multiple custom attributes and non-list values, remote and
    `apply/3` calls, `badarg` for an unknown key; values that differ
    (`md5`, `compile`, missing-`vsn` integer) are checked by shape only.
  - [x] Lint goldens (`tests/fixtures/lint`) for the diagnostics OTP shares
    (hand-written definition, explicit export); CLI cases for the rest.
- Evidence (2026-10-10): `executables_module_info` (OTP 29.1.1 stdout),
  lint cases `module_info_defined`, `exports_repeated`; fresh fast CTest and
  check-quality ([validation](../docs/validation.md#history)).

<a id="step-65c"></a>

### 65C. Import functions, accept compile hints, name rejected attributes

Backlog: D03 (selected slice). Depends on: [65B](#step-65b). Added 2026-10-10
(user direction, from compiling luerl); runs right after 65B.

Real projects such as luerl stop on `-import(Mod, [F/A])` of modules other
than `erlang` and on optimization-only `-compile` options, with an
unspecific `[behavior-changing attributes] notimpl`.

- Success criteria
  - [x] `-import(Mod, [F/A, ...])` of any module: a local call `F(...)` of an
    imported function, which the module does not define, is the remote call
    `Mod:F(...)` (literal module, library loading and remote export rules
    apply); an import overrides an auto-imported BIF of the same name and
    arity as in OTP. erl_lint's conflicts are reported with OTP's wording:
    importing one function from two modules, and defining an imported
    function. `fun F/A` of an imported function follows OTP.
  - [x] Optimization and reporting hints in `-compile` are accepted and
    ignored: `inline` and `{inline, [F/A]}`, `{inline_size, N}`,
    `{inline_effort, N}`, `inline_list_funcs`, `debug_info`, `deterministic`,
    report/verbosity options and `warn_*` beside the existing `nowarn_*` and
    `no_auto_import`; options that change meaning (`export_all`, parse
    transforms, `{d, ...}`, unknown ones) stay rejected.
  - [x] A rejected attribute names itself and the offending option, such as
    `-on_load attribute`, `-compile option {parse_transform,eunit_autoexport}`
    or `-import of erlang:foo/1` (an import of an `erlang` function that is
    no builtin), instead of only the capability.
  - [x] Contracts in `docs/semantic.md`/`docs/compile.md`, differences, step
    73 and the backlog updated.
- Tests
  - [x] Executable golden: imported library and batch functions called
    locally, in guards where OTP allows, through `fun F/A`, and an import that
    overrides an auto-imported BIF.
  - [x] Lint goldens for the import conflicts OTP reports; CLI cases for the
    accepted hints and the named rejections.
  - [x] luerl's `src/` gets past attribute admission (recorded, not a gate).
- Added 2026-10-10 (user direction): `{parse_transform, Module}` is accepted
  and not applied, with a `[parse transforms] notimpl` warning (feature ID
  27); applying transforms stays with [73](#step-73).
- Evidence (2026-10-10): `executables_imports` (OTP 29.1.1 stdout), lint cases
  `imports_conflicts`, `imports_fun`, `imports_bif`, CLI cases; luerl `src/`
  passes attribute admission and stops on missing library modules and
  builtins (`array`, `ordsets`, `math`, `io_lib`, ...). Fresh fast CTest and
  check-quality ([validation](../docs/validation.md#history)).

<a id="step-66"></a>

### 66. Validate macOS Apple Silicon

Backlog: V01. Depends on: [63](#step-63).

- Success criteria
  - [ ] Same as step 63 on macOS arm64.
- Tests
  - [ ] Full gate plus fixture goldens.

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
    `on_load`, parse transforms (accepted unapplied with a warning since
    65C), others; `behaviour` is implemented by [65A](#step-65a),
    `module_info` and informational attributes by [65B](#step-65b), imports
    and compile hints by [65C](#step-65c));
    rejected ones keep diagnostics.
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

<a id="step-78b"></a>

### 78B. Rename runtime builtin modules to builtin_<name>

Backlog: all. Depends on: [78A](#step-78a). Added 2026-10-09 (user
direction).

Rename the C++ sources and headers of `runtime/src/builtins/` to
`builtin_<name>.cpp` / `builtin_<name>.hpp` (`erlang.cpp` to
`builtin_erlang.cpp`, `io_format.hpp` to `builtin_io_format.hpp`, ...) and
update includes, CMake lists, `files.md`, `arch.md` and every document
naming them.

- Success criteria
  - [ ] Every file of `runtime/src/builtins/` is named `builtin_<name>`; no
    reference to an old name remains.
- Tests
  - [ ] Fresh full gate and `check-quality-all` pass.

<a id="step-78"></a>

### 78. Publish the final implementation and validation boundary

Backlog: all. Depends on: steps 1–70, [78A](#step-78a), [78B](#step-78b) and any selected optional work.

- Success criteria
  - [ ] `00-finished.md`, `01-todo.md`, `arch.md`, `files.md`, contracts and
    examples reflect exactly what is implemented, validated, deferred or
    omitted.
  - [ ] README shows building and running a multi-process Erlang program as an
    executable.
- Tests
  - [ ] Fresh full gate on every available host; README commands executed as
    written.
