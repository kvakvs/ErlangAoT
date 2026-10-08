# Architecture

- C++23, project-internal APIs, CMake, warnings as errors. `erlangaot` owns the
  compiler pipeline; `erlang_runtime` is separately linkable and LLVM-free.
  Stage boundaries exchange owned internal data. Public interchange and readers
  remain deferred; only reader directory locations are reserved.

- SourceManager owns UTF-8/Latin-1 buffers. Incremental lexer/directive/preprocessor
  stages retain decoded values, physical/logical spans, macro/include ancestry and
  feature snapshots. Per-session macros and include/conditional stacks preserve
  rescan/stringification. PP conditions use their own closed arbitrary-integer,
  exact-order and bitstring evaluator; this does not define executable guard legality.

- ParserSession consumes expanded forms transactionally. Flat owned AST arenas
  use owner/generation-checked IDs, closed variants and retained token provenance.
  Token/node/depth/work/diagnostic limits stop exhaustion and roll back partial forms.
  Syntax covers ordinary OTP 29 grammar, including containers, records, guards,
  control/funs/exceptions/maybe and comprehensions. Syntax acceptance is separate
  from executable admission; there is no textual AST reader.

- Positional/project drivers retain complete target AST batches, run common
  semantic/backend stages and aggregate located diagnostics. Target frontend
  options and results remain isolated. Syntax-only actions stop early; default
  compilation emits in memory; publication waits for every selected target.
  Implementation/debug/progress reporting is bounded and opt-in.

- LLVM-free semantic indexing validates exact module/function/export identities.
  Clause/local binding IDs distinguish definitions, reads and exact checks;
  tentative candidates publish only on success. `_` defines nothing; `_Name` is
  ordinary. Body '=' analyzes RHS first. Pattern siblings share incoming reads;
  only a binary's own preceding segments extend its size scope. Bounded flat
  normalization owns constants/source anchors; semantic failure clears partial tables.

- Current execution: ordered clauses, scalar/tuple/list/string/map/bitstring/expanded-record patterns, grouped
  guards and body matches/sequences, constructors, checked access/comparison, boolean
  operators and local/exported batch calls, including recursion: `resolve_calls` orders strongly
  connected components callees-first (iterative Tarjan) and inference iterates recursive components from
  `none()` to a fixed point, widening every member to `term()` after 16 rounds (step 18). Flat match plans carry explicit success/mismatch continuations and tentative
  SSA values. Each candidate owns fresh bindings; head/guard rejection advances with original arguments. All clause bodies feed call/inference/atom/inspection analysis. Result joins preserve only common argument relations. Checked equality is representation-aware; exhaustion
  raises function_clause. Body matches save the RHS once and reuse the matcher; only success publishes bindings, while badmatch retains the RHS and exits before later work. Unconditional heads retain direct argument projections inside their root scope.
  `case` (step 9) reuses the one-input body plan per clause over the scrutinee, guards via `lower_guard`, and the
  iterative walker (`case_select`/`case_clause_end` actions) restores the entry bindings per clause and joins the
  value plus every exported binding with PHIs; exhaustion raises `{case_clause, V}` (ErrorReason 9). Binding
  analysis gives a name bound by several clauses one identity (`BindingAnalysis::branch_names`) and records
  exports in `Function::exports`; partial definitions become unsafe. `begin`/`end` is a plain sequence.
  `if` (step 10) takes the same paths with no scrutinee or pattern: `semantic::branch_clauses` gives every
  consumer one case/if clause view; exhaustion raises the atom-only `if_clause` (ErrorReason 10).
  Source raises (step 11): `error/1,2,3`, `exit/1`, `throw/1` are body builtins (`semantic::body_builtin`,
  unqualified via auto-import unless shadowed or suppressed) lowered by `lower_raise` to `erlang_aot_raise_v2`
  with `raised_error/exit/throw` (11-13); the payload is the whole reason and the ID the class.
  `catch Expr` (step 12): the walker sets `ExpressionLowering::handler` (and fresh badarg/badarith exits) while
  `Expr` lowers, so `propagate_failure` and `raise_reason` branch to it; the handler calls `erlang_aot_catch_v1`
  (runtime `process/exceptions`: catch value, clears the channel) and re-checks to reach the outer exit for
  halts/runtime failures; a PHI joins the value and the pre-catch bindings are restored (inner names unsafe).
  `try ... of ... catch` (step 13): `semantic::branch_clauses` lists of clauses then catch clauses (`Branch::handler`,
  `first_handler`); the walker protects only the body (`ProtectedScope`), selects of clauses on its value
  (`try_clause` on exhaustion), and from the handler takes `{class, reason}` (`erlang_aot_exception_v1`) and matches
  catch clauses from the pre-try bindings; no match re-raises (`erlang_aot_reraise_v1`). All try names stay unsafe.
  `try ... after` (step 14): a second `ProtectedScope` (`afters`) encloses body and clauses; after the join the after
  body is lowered on the normal path (value kept in `AfterPath`), and, when its handler is used, again from the
  handler between `lower_exception` and `reraise`.
  Stack traces (step 15): `begin_roots` names a private name descriptor (module descriptor, module/function atom
  slots, arity) copied into the function's `FrameDescriptor`; `GeneratedCallState::fail` copies `ProcessStack::trace()`
  (8 innermost named frames) into `CallFailure::trace` for Erlang exceptions without a given `stack`. Terms are built
  lazily (`stack_term`); `Exception` = {class, reason, stack}; reraise/raise3 store the stack term in
  `CallFailure::stack`; `error/2,3` keep `CallFailure::arguments` for the top frame. Both are process roots.
  `maybe` (step 16): AST keeps `MaybeMatch` items; `semantic::maybe_operands` lists body values, `branch_clauses`
  gives else clauses (`first_handler` 0, so binding analysis treats them like catch clauses inside one conditional
  scope). The walker's `MaybeScope` collects `?=` mismatch edges into `maybe.else`; a PHI of unmatched values feeds a
  `CaseJoin` whose else clauses reuse case selection (`no_match` raises `else_clause`).
  List comprehensions (step 21, `codegen/lowering_comprehensions`): `semantic::comprehension_*` views qualifiers;
  binding analysis saves/restores the scope and binds generator patterns as a fresh (shadowing) candidate per
  qualifier (zip groups together); guard-test filters (`Function::guard_filters`) lower as guards. Each generator is a
  loop whose cursor and the reversed accumulator live in term slots (no PHIs); the result is reversed once by
  `ContainerConstruction::reverse`. Errors `bad_generator`/`bad_filter`/`bad_generators` (ErrorReason 16-18).
  Step 22: binary generators use match plans with `GeneratorPattern::element` (tail segment -> `MatchPlan::rest`)
  and `skip` (OTP skip pattern); map generators keep map/position/size slots (`MapOperation::key_at/value_at`);
  each step tries acc, then skip (relaxed: advance; strict in a zip: rematch), then exhaustion, then the error.
  Binary/map producers finish with `BitOperation::concat` / `MapOperation::from_list`.

- Execution model (step 17 decision, implemented in step 19; `docs/execution-model.md#implementation`):
  lowering emits native form (`Word f(ctx, args)`, ordinary calls, `erlang_aot.frame` slot marker, `erlang-arity`
  attribute, tail calls as `ret call`); `codegen/frames` (`lower_frames`, run by the backend before inspection and by
  `optimize`) moves each body to `<sym>.body` (`void(ctx)`), prologue = frame header/registers services + resume
  switch, splits after non-tail calls, spills cross-call SSA values to raw slots, hoists constant slot GEPs, and emits
  only `musttail` transfers via `erlang_aot_enter/tail/return_v1`. Descriptors `<sym>.frame` (7 words: names, arity,
  body, slots, roots); exported `<sym>` = host wrapper over `erlang_aot_invoke_v1`. Runtime `ProcessStack`
  (`process/stack`): flat `std::vector<Word>`, 4-word headers linked by offsets, 256 X registers, uncapped
  by default (opt-in `StackOptions::limit_words` -> `resource_limit`; host refusal `out_of_memory` -> exit 70), bottom frame per invocation contains native exceptions. Exceptions still return
  through callers (channel check). Step 43 yields: `ProcessStack::enter` spends a reduction per entry (4,000 per
  slice); at zero it records `resume_` = the entered frame, keeps its argument registers and returns `pause`, so
  the musttail chain unwinds to whoever ran the slice (`ProcessStack::run`). Prototype `tests/prototypes/execution_model/`.

- Ordinary record layouts retain declaration order, defaults and source provenance.
  Bounded per-use expansion reuses tuple matching and rooted construction. Checked
  access validates tag/arity; guard mismatch rejects, body badrecord owns its payload.
  Updates evaluate values, then the record, check it, copy the other fields; `record_info/2` folds to
  constants. Local native records (31C) lower to `erlang_aot_record_v1` with the module's
  `<prefix>.records` descriptor table; patterns plan `record_test`/`record_field` nodes; construction
  evaluates explicit fields in source order, native updates the record first. 31D: `-export_record`/
  `-import_record` (`Module::exported_records`/`imported_records`), `semantic::external_record` resolves
  qualified and imported names; `Module::peers` (set by call resolution) lets external construction find
  the defining module, lower its literal defaults here (`ExpressionLowering::atom_owner`) and reference its
  external `<prefix>.records`. 31E: anonymous `#_` access (check any), update (`exported_or_module`) and
  patterns (`any` without fields, else `exported_or_module`); `#_{...}` as an expression is an error.

- Function values (step 32, `docs/funs.md`): `semantic::index_funs` (end of binding analysis) gives each distinct
  `fun F/A` / literal `fun M:F/A` a `Module::funs` entry (local funs numbered in source order); registration emits
  `<prefix>.funs` (`abi::v1::FunDescriptor`: atom slots, arity, index, external flag, entered `FrameDescriptor`,
  null for an external fun the batch does not export), bound to runtime `FunDefinition`s. Cells `fun_closure` =
  header, untraced `const FunDefinition *`, captured values. `F(Args)` (`semantic::fun_call`: target neither atom
  nor remote) evaluates the target first; lowering stores the arguments in an array, calls `erlang_aot_apply_v1`
  (badfun/badarity/undef, appends captures, returns the frame) and the `erlang_aot.apply` marker, which
  `lower_frames` turns into an enter/tail transfer with the array as the registers. Funs order after atoms;
  local < external. Builtin funs and `fun M:F/A` with variables stay `dynamic calls`.
  Closures (step 33): binding analysis gives each anonymous fun clause the scope at the fun (fresh shadowing heads,
  `FunScope` hides case branch names) and records `Function::captures` (outer identities used inside, definition
  order); `expression_children` lists fun guards and bodies so all walks see them, except the codegen walker.
  `index_funs` names lambdas `-f/A-fun-N-`, symbol arity = arity + captures; `lower_lambda` lowers the clauses into
  that private native function (`ExpressionLowering::lambda` picks clause plans, frame names and arity) with the
  captured values loaded from the arguments after the fun's own; `lower_fun` roots captures for the make service.
  Named funs (step 34): `FunScope::inside` = scope at the fun + the name (a definition anchored on the fun
  expression, no binding event, `Function::fun_names` -> `FunEntry::self`); `name_self` in `lower_clauses` builds
  the fun from its own descriptor and captured arguments once on entry when a clause reads the name and keeps it
  rooted like an argument; `Name(...)` is a plain fun call (tail call in tail position).
  Dynamic calls (step 35, ABI 8): `ExportDescriptor::frame`; registration binds `ModuleAtoms::module/exports`.
  `semantic::dynamic_call` (non-literal module or function) is no direct call; its children are module, function,
  arguments. `apply/2,3` resolve as body builtins (`ServiceResolution::apply()`, no immediate operation). Codegen
  `transfer` shares the fun-call path: a preparation service returns the frame (`erlang_aot_call_v1`,
  `erlang_aot_apply_list_v1`/`erlang_aot_call_list_v1` unpack the list into a 256-word register array), then the
  apply marker. `fun M:F/A` with variables: binding reads in `Function::fun_operands`, built by
  `erlang_aot_make_external_fun_v1` over `CodeServer::external_fun` (interned, `owns()` admits it).
  Builtin bridge (step 36, `docs/builtins.md`): append-only ABI catalog `abi::v1::bridge_builtins` shared by compiler
  and runtime; `CodeServer::builtins()` (`BuiltinRegistry`, transactional batches, registered at runtime startup
  from `erlang_builtins()`, adapters over the inline services) maps names to `BuiltinFrame`s = `FrameDescriptor`
  with null body + `BuiltinBody`. `ProcessStack::enter` runs a null-body frame at once on the registers and returns
  into the caller's body. Lookups: `CodeServer::function_frame` (exports, then builtins by atom spelling) for
  `M:F(Args)`/`apply/3`/runtime `fun M:F/A`; registration binds external `FunDescriptor`s without a frame to the
  builtin of their name. Compiler: catalog builtins without an inline operation (`function_exported/3`) are body
  builtins with `ServiceResolution::builtin` lowered to `erlang_aot_builtin_v1`; `fun F/A` of an auto-imported
  catalog builtin is recorded by `resolve_services` (`Function::builtin_funs`) and becomes the external entry
  `erlang:F/A` (`add_builtin_fun`); `halt/0,1` auto-imported. Step 37: term-access family (`term_access_builtins()`:
  `setelement`, `make_tuple/2,3`, `tuple_to_list`, `list_to_tuple`, `'++'`, `'--'`); `A ++ B`/`A -- B` lower to the
  bridge (`binary_value`), so the `arithmetic` capability is implemented. Step 38: conversions (`conversion_builtins()`, float text in
  `builtins/float_text`: printf for `%.*e`, OTP's own fixed rounding, shortest digits from `std::to_chars` placed by
  OTP's Ryu notation rules). Step 39: library modules (`library/stdlib/{lists,maps}.erl`, original Erlang) join a
  batch in `driver/frontend` `add_library`: modules named by literal atoms (`semantic::referenced_modules`) that no
  input declares (`semantic::declared_module`) are parsed from `linking::library_directory()` (relative to
  `erlangaot`, `ERLANG_AOT_DEFAULT_LIBRARY`) until closed; they compile, link and publish like inputs.
  Step 40 (`docs/io.md`): catalog entries of module `io` (`io_builtins()`); a qualified call of another module's
  catalog builtin is a service (`semantic::module_builtin`) ahead of call resolution. Runtime `builtins/io_format`
  (scan, control sequences, column tracking, chardata walks), `builtins/io_pretty` (OTP intermediate form with
  one-line lengths, then pp/cind layout; sequences iterate, nesting recursion capped at 256 -> system_limit),
  shared `builtins/text` (UTF-8, digits); text is built as `std::u32string`, validated, written as UTF-8.
  `TermStyle::write_unicode` = `~tw` atoms.
  Step 41: typed builtins (`builtins/typed.hpp`): `typed<Function>` adapts `Result(ProcessContext &, Params...)`
  to a BuiltinBody (admit, convert via `Argument<T>`, mismatch -> badarg, publish Term/TermResult/BuiltinResult/Word;
  thrown `BuiltinFailure` recorded). term_access, conversions, io, binary_part/2, function_exported/3 use it;
  the other erlang adapters forward raw words to the inline services.
  Step 42 (`docs/terms.md#pids-and-references`): pids are immediates (tag `0x3`, payload = process number from one
  process-wide never-reused sequence; `detail::ProcessNumbers` in `Runtime::Impl` records issued numbers as runs,
  `ProcessIdentity::serial_` is the number); admission (`TermAccess::pid`, `HeapStorage::processes_`) rejects
  forged/foreign words, exited pids stay valid. References are `reference` heap cells (untraced 64-bit number from a
  program-wide counter). Order: numbers < atoms < refs < funs < pids < tuples; print `<0.N.S>`/`#Ref<0.A.B.C>`.
  Builtins `self/0`, `make_ref/0` (`builtins/processes`), `pid_to_list/1`, `ref_to_list/1` (conversions); body
  `self()` resolves its guard signature to the bridge builtin (`guard_analysis` `call`), guard `self()` stays gated.
  Step 43 (`docs/processes.md`): `detail::Executor` (`scheduler/executor`, member of `Runtime::Impl`, reached by
  `Executor::of(context)`) = FIFO deque of runnable contexts; `run(main)` runs slices round robin until main ends or
  another process halts/fails outside Erlang, releasing other ended processes at once. `Runtime::Impl::contexts` is a
  map by pointer, `processes` a map by pid number (`is_process_alive/1`). `spawn/1,3` (`builtins/processes`) create
  a context, copy the fun/args into it and prepare the first call in the child with the dynamic call services
  (`apply_list_service`/`call_list_service`), so badarity/undef crash the child. Startup queues the entry frame as
  the main process; host `invoke` resumes its own yields without running other processes.
  Step 43A (`docs/builtins.md#portions`): body bridge builtins lower to `erlang_aot_builtin_frame_v1` + the apply
  marker (entered like functions; body `length/1` too, `guard_analysis` `portioned`). A builtin portion spends
  `ProcessStack::budget()` units (16 per reduction left), then `trap(continuation, state registers)`; `enter` takes
  the trap, zeroes reductions and suspends at the continuation (`continuation_frame`). Native state = `TrapState`
  (`words()` are roots in `ProcessStack::visit`). `call_builtin_portion` (enter) vs `call_builtin` (host: loops).
  `builtins/lists` (`length`, `++`, `--`: collect, merge sort, binary-search scan, build) and the binary/iolist
  conversions over `builtins/portions` (`walk`, `build_list`, `guarded`, `ListState`); `TermFactory::list_words`.
  Step 44 (`docs/processes.md#exits`): `process/exits` `exit_reason` (normal, exit reason, {R, Stack},
  {{nocatch, V}, Stack}) and `report_exit` (OTP legacy `=ERROR REPORT====` text on stderr, `~p` via
  `builtins::pretty`), called by the executor for an ended non-main process; `stack_term` is public in exceptions.
  Step 45 (`docs/processes.md#messages`): `!`/`send/2` bridge builtins -> `Executor::send` (copy into the receiver's
  heap, `Mailbox::deliver`). `Mailbox` (`runtime/include/mailbox.hpp`, `process/storage.cpp`): `std::list` inbox and
  queue + saved position (`peek` splices arrivals, `skip`, `take`, `restart`, `unexamined`); words are roots.
  Step 46 (`docs/processes.md#receive`): receive = `branch_clauses` case on the message; walker `receive()` builds
  loop (peek into root slot) -> match (clauses; `take` before body; last mismatch `skip` + br loop, `CaseJoin::loop`)
  / wait (`lower_wait` = call transfer into `erlang_aot_wait_frame_v1`'s builtin, then br loop). Runtime
  `process/receive`: wait -> `ProcessStack::wait` (trap + waiting); executor `parked_`, send wakes, empty queue
  blocks forever, `slice()` runs one process.
  Step 47: after body = last `branch_clauses` clause (no pattern/guard; `first_handler` = message clauses); timeout
  evaluated first; wait -> true (scan) / false (`receive.timeout`: `restart`, after clause). Mailbox `deadline_`
  (first wait, cleared by take/restart); executor `timers_` multimap, `expire()` per slice, `idle()` sleeps;
  `ErrorReason::timeout_value`. Timer wheel planned (62B).
  Step 48 (`docs/processes.md#links`): `Signals` (`runtime/include/signals.hpp`, `ProcessContext::signals()`): link
  pid vector + `trap_exit`. Executor signal code in `scheduler/signals.cpp`: only the running process sends signals,
  so targets are queued/parked and handled at once: `signal()` -> `end()` (copy reason, record
  `CallError::exited` + raised_exit; running process unwinds past catch/after; others `withdraw` + `ending_`),
  `deliver_exit()` ({'EXIT',From,R} + wake) or drop. `slice()` -> `finish()` (main/program-ending -> `stop`,
  `finished_`; else `notify_links` with `exit_reason`, `report_exit`, destroy) then `drain()` the `ending_` deque.
  `running_` (with `Running` scope for host invocations) marks the process that unwinds instead of being withdrawn.
  `ends_program` excludes `exited`; startup reports an exited main as uncaught exit (normal -> 0).
  Step 49 (`docs/processes.md#monitors`): `Signals::monitors_` (held: ref -> target pid) and `watchers_` (held on
  it: ref -> watcher pid), `std::map<ReferenceIdentity, Word>` (ordered by creation). `ReferenceIdentity` (number,
  from `Term::reference_value`) rebuilt by `TermFactory::reference` (make_reference delegates). `Executor::monitor`
  / `demonitor`; `notify()` drops held monitors, then signals links and delivers 'DOWN' via `deliver()`;
  `Mailbox::remove(match)` for demonitor flush.
  Step 50 (`docs/processes.md#registered-names`): executor `names_` (std::map atom word -> pid; `whereis` checks
  liveness via `find`), `Signals::name_`; `notify()` erases the name first. `Signals::Monitor{pid, name}`: a name
  monitor's 'DOWN' item is {Name, nonode@nohost} (`LOCAL_NODE`). Sends: `destination_pid` (pid, atom -> badarg if
  unregistered, {Name, Node} -> local lookup or drop).
  Step 52 (`docs/guards.md#catalog`): immediate ops `is_native_record` (after `is_function`, predicate range),
  `self`, `node`, `node_of` (after `maximum`, `identity()` in runtime `immediate_services`); zero-arity service calls
  pass the empty list as operand. Bridge builtins is_record/1, node/0,1 (funs, apply). The `guards` notimpl path
  (services without lowering) is gone; feature `guards` implemented. Test placeholders for "dynamic calls" use
  `fun erlang:apply/2`.
  Step 53 (decision, `docs/processes.md#ports`): no ports. Feature `ports` (id 26, deferred);
  `semantic::port_builtin` names the port BIFs; `calls.cpp` `port_call` (local undefined or erlang:F) and
  `check_reference` (fun F/A), `expression_capability` (fun erlang:F/A) report "ports". Dynamic calls: undef.
  Step 54 (`docs/runtime.md#threads`): `AtomStorage` behind one shared mutex (shared lookups, exclusive insert
  with re-check); `runtime_concurrency` stresses it from 8 threads. Step 55: `CodeServer` likewise (modules_,
  external_funs_; private unlocked `find_*` helpers); modules never unloaded, so returned pointers stay valid.
  Step 56 (`docs/processes.md#workers`): `Executor::run` starts `RuntimeOptions::schedulers - 1` threads and
  runs `work()` on each (one shared FIFO queue, `mutex_`, `work_` condition variable, idle waits until the earliest
  timer). Per-process `Schedule` (running, blocked_on, blockers, holds/holding, ready, ending) in `schedules_`;
  thread_local `running_`. Signals' links/monitors/name are executor-guarded; heap/mailbox/failure of a process
  running elsewhere are not touched: `send`/`exit` throw `builtins::Blocked`, the typed adapter traps to
  `Adapter::RETRY` (itself) and `after()` parks the sender among the target's blockers; at the target's slice end
  it is placed first, then blockers are held/queued at the front (`hold`, `place`, `resume`). `finish()` defers
  an ended process with a busy peer (`busy_peer`). `RuntimeMemory` atomic, `ProcessNumbers` shared mutex.
  Step 57: no code; wakeup/shutdown argument in processes.md#workers, stress golden `executables_wakeups`.
  Step 57A (decision `docs/ports.md`): port immediate 0x7, executor port table, drivers fd/spawn/file/tcp/udp, one
  I/O thread per runtime (IOCP / poll()), events delivered under the executor mutex; prototype tests/prototypes/poller.
  Step 57B: port words (tag 0x7) from `IdentityNumbers` (pid + port sequences), executor `ports_` table of `Port`
  (driver, connected, links, watchers, name, options, counters), `scheduler/ports.cpp` (open/close/command/connect/
  request/info, port exit rules, `PortEvent` post/apply for running targets), `builtins/ports.cpp` (13 BIFs, iodata,
  options, `port_request` parsing for sends), `ports/fd.cpp` (output-only fd driver, framing). Executor holds
  `Runtime::Impl &runtime_` (`process(pid)`); `signal_messages.hpp` shares EXIT/DOWN builders and `Running`.
  Step 57C: `IoService` (`ports/io.hpp`; `io_posix.cpp` poll thread, `io_windows.cpp` reader threads) with
  `InputDecoder` framing, delivering (port, bytes read, units) to `Executor::input` under the mutex; data becomes
  `PortEvent::data` messages; executor `io_` declared last, stopped outside the lock in `clear()`.
  Step 57D: `IoService` base (io.cpp) adds detached writer threads (queued output) and child watchers (status units)
  sharing an `IoGate`; `PortDriver::queued_output/child/owns_descriptors`; spawn driver (`ports/spawn*.cpp`);
  executor `exited`/`finish_input` order exit_status before eof/close; `os` builtins + library `os:cmd/1`.
  Step 51: no code change; non-running processes are never collected, a resumed process collects at its resume
  entry safepoint (wait builtin / trap continuation / yielded function); `executables_mailbox_collection`.

- Guard authorization uses the fully audited pinned legal name/arity/operator catalog, separately
  from availability. Explicit erlang calls, local shadowing, imports, no_auto_import
  and top-level legacy tests are resolved before every operand is traversed, including
  skipped operands. Grouped comma tests require canonical true; semicolon rejection
  continues at the next alternative. Guards cannot create bindings. Compound range tests validate arbitrary-integer bounds before candidate comparison. Only self/node and native is_record/1 signatures remain dependency-blocked.

- Iterative eager/lazy lowering preserves source order without host recursion.
  Strict and/or/xor/not validate booleans; andalso/orelse validate the reached left
  operand and join term-valued results through target-word SSA. Reached semantic
  errors reject the enclosing guard alternative; resource/ownership/internal
  failures stop all recovery. Body errors raise badarg or owned {badarg,Value}
  for an invalid lazy left operand. Join instructions retain operator provenance.
  See docs/{patterns,guards}.md.

- Runtime Terms admit exact arbitrary integers, owned atoms, tuples and proper/improper lists. Atom storage validates UTF-8, deduplicates spelling and enforces limits.
  Globally non-recycled words reject foreign ownership; immutable pins retain
  host/error spellings after teardown. Revision-4 module descriptors initialize
  deterministic atom slots before registry publication. Generated reads/booleans
  never intern on evaluation. Iterative structural order uses decoded integers, atom spelling, tuple arity/fields and cons heads/tails, with no work cap (27B). Integers follow the ERTS size limit (`BIG_ARITY_MAX` words); a larger result is service outcome `system_limit`: guards reject, bodies raise `error:system_limit` (27E).
  Failed registration may retain valid atoms, but publishes no module/slots.

- Integers normalize target-sized values to immediates and store larger immutable
  sign/magnitude words in the process heap. Owned bounded multiprecision temporaries,
  explicit word codecs/carry/borrow and double-width LLVM fast paths preserve exact
  promotion/demotion. Numeric semantic errors reject guards; badarith/abs badarg
  and infrastructure failures retain their separate body/channel outcomes.

- Finite binary64 values use float heap cells and rooted literal services.
  Numeric conversion has explicit rounding/range rules; mixed comparisons avoid
  rounding arbitrary integers, and exact equality preserves signed zero. Shared
  body/guard services retain semantic versus infrastructure failure outcomes.

- Immutable maps store canonical exact keys and stage updates before publication.
  Computed-key patterns use incoming bindings and checked rooted lookup; map
  comparison separates exact keys from contextual value comparison.

- Bitstrings own exact MSB-first bit sequences with zero tail padding. Small
  construction uses inline cells; large buffers and extracted views share immutable
  backing. Checked numeric/UTF builders stage before publication. Flat matching
  carries explicit cursors and preceding-segment size scopes; native endian derives
  from the LLVM target. Queries, parts and structural order share this representation.
  Borrowed service arrays and both success outputs are rooted. Dead cells are reclaimed by a
  collection; a buffer is freed with its last cell.

- Classic ERTS process heap (`docs/runtime-heap.md`, phase C 8A-8I). `HeapStorage` = one heap block
  `heap_` (lazy, `max(min_heap_words=233, request)`) + `fragments_` (newest tried after the heap, else a
  new one) + off-heap list of `RefcBinaryCell`s (`memory/off_heap`); bump allocation, word alignment,
  one reservation with `HeapMark` rollback. Words are header-parsed (`memory/heap_walk`,
  `ProcessHeap::verify` in `memory/heap_verify`); admission = owned range (heap, then fragments by
  address, below top) + header shape; process pointers only name object starts. Host `Term` = word +
  borrowed heap + weak lifetime + collection count (no pin). Roots (step 23 inventory in runtime-heap.md) =
  frame term slots of `ProcessStack`, `keep_registers` live X registers, failure payload/arguments/stack,
  explicit span (`ProcessContext::visit_roots`); raw spill slots are never roots. `memory/heap_collect`
  `Copier`: Cheney copy of heap+fragments into one new block at a safe point (`ProcessHeap::collect(roots)`
  outside generated code, or inside a `SafePoint` scope). Generated code collects at function entry
  (`ProcessStack::enter` -> `safepoint(arity)`) and comprehension loop heads (`erlang_aot_safepoint_v1`) when
  `wants_collection()` (fragments, or off-heap words >= `binary_limit_words_`); all services are critical
  sections. `lower_frames` spills crossing terms to term slots (`term_value`, `place_slots`). Forwarding words, off-heap sweep, ERTS size sequence, second copy
  to shrink a block under 25% live or above `block_limit` (step 27: a block and the virtual binary heap keep
  half of the budget left after survivors free, so garbage triggers a safepoint before `limit_exceeded`;
  no default heap or stack cap: opt-in per-process budgets, host refusal = `out_of_memory`, exit 70; step 27A:
  optional runtime-wide limit, one `detail::RuntimeMemory` account charged by heap blocks, fragments,
  off-heap buffers and stack capacity, seen by each process as budget = own storage + what the limit leaves;
  programs set `--max-heap/--max-stack/--max-memory`). Step 28: `ProcessHeap::add`/`copy_to` copy a foreign
  same-runtime graph (`memory/copy` `GraphCopy`: iterative discovery keyed by address keeps sharing, one
  reservation, refc cells share buffers and link after commit); factories use `retain` (foreign = wrong_owner).
  Buffers charge the runtime account once (deleter releases); each process counts cells per buffer
  (`HeapStorage::buffers_`) and charges a buffer once to its own budget. Step 31B: native record cells
  (`native_record`: header, untraced `const RecordDefinition *`, values in definition order); module
  registration binds `abi::v1::RecordDescriptor`s into `ModuleAtoms::records` (code server
  `record_definition`); `erlang_aot_record_v1` (`terms/record_services`) makes/gets/updates/matches/tests
  under a `RecordCheck`; walker slots skip the definition word. Revision-6 generated frames
  hold arguments/temporaries in term slots and clear failed candidates; invocations restore the stack
  after native exceptions. Constructors publish initialized cells
  transactionally; backing allocation failure rolls back. Rooted runtime scratch buffers keep wide
  source constructors off the native stack.

- Generated ABI entries retain target-word terms, context and argument arrays.
  Revision-2 first-error channels separate structured Erlang errors from exact
  infrastructure statuses. Every fallible call/service checks before output use;
  nested invocation scopes preserve first failure and outer cleanup permits retry.
  Native C++ service symbols follow target platform/width. Frozen registries and
  resolved handles pin code images/module bindings. Host mutation is serialized.

- Symbolic type graphs and declarations remain separate from implementation facts.
  Bounded joins widen to top; aliases/opaque identities remain finite and scoped.
  Specs never authorize representation checks. Inference indexes clause-local reads and propagates justified whole-value body
  assignment/alias facts. Extracted/unproved values remain top, candidate joins
  retain common relations, and indexing/publication share the inference budget.
  Lowering borrows one read index but retains isolated candidate SSA maps.
  Specialization uses actual profiles and guarded generic fallback: 3 variants per
  function, 32/module, 128/target and at most 2x measured generic IR. Current source
  offers no profitable removable checks; the admitted-domain proof audit passes
  without adding speculative check removal.

- LLVM lowering uses target-derived layouts, collision-free symbols and checked
  runtime services. Standard O0/O2 PassBuilder pipelines verify fresh batches on
  both sides; -Os is O2 over optsize definitions with function/data sections and
  linker dead-stripping (runtime also built with sections). Text/bitcode/object serialization owns bytes; source-scoped line
  metadata and bounded comments annotate textual IR. Artifact publication checks
  encoded paths/aliases, stages exclusive writes and replaces complete files;
  multi-file publication is not atomic. Startup objects exist; positional `-o` links executables.

- Runtime lifecycle/context ownership, generic registration, stable backing and roots
  are implemented. GC, workers, messaging, process identities,
  dynamic loading and further builtin families remain with their named owners.
  Patternmatch steps 1–20 and step 15a are complete within function-clause/body-match scope.
  History is condensed in docs/validation.md#history.

- Tests prioritize real CLI/project sources and separate native runtime consumers.
  Local source fragments are separate from retained OTP observations; 19 corpora preserve 67,634
  native expected values in all eight driver/policy combinations, plus 106 semantic
  rows. `fixture_sources` prevents committing OTP notices/source outputs; regeneration
  stages local fragments and observes them with OTP. OTP source/generated headers stay
  in ignored reference/build directories. The seeded closure corpus reconciles every manifest/catalog mapping. Live OTP/source audits are explicit opt-ins, while
  normal configure/build/test requires neither OTP nor its checkout. Grammar coverage observes 344 ordinary productions;
  suite parsing/foreign objects never count as native semantics. Focused private
  tests cover inaccessible budgets, ownership and injected faults. Fresh combined
  Windows x64 Debug passes 125 OTP-free CTests and all 258 production quality units;
  formatting remains mandatory. Other native hosts/32-bit and new frontend sanitizer
  runs remain unavailable. Historical foundational macOS/runtime-ASan evidence stays
  in Git history; summary in docs/validation.md.

- Executable entry: `--entry`/manifest `entry` or the sole `main/1` exporter, resolved in
  `driver/entry` right after semantic indexing; contract in `docs/executables.md`. `#!` sources are escripts: `driver/escript` rewrites
  the header, `semantic/escript` exports `main/1`; entry detection prefers them.
- Startup: a resolved entry sets `CompilationRequest::startup`; `codegen/startup` appends a
  module (after the inputs, no syntax; artifact `eav1_start`) whose `main` hands a
  `StartupDescriptor` to runtime `erlang_aot_main_v1`. The runtime ABI-checks all descriptors,
  parses runtime options (`ERLANG_AOT_FLAGS`, then leading `--max-atoms`/`--args-file`/`--`, `startup/options`),
  registers every module before entry, builds argv, runs the entry in one context and maps
  return/halt/exception/infrastructure outcomes to exit 0/N/1|127/70. `erlang:halt/0,1` records
  `CallError::halted` in the checked channel, so halts unwind like errors.
- Linking (`linking/`, LLVM-private): positional `-o` keeps objects in memory, stages them in a
  private `.erlangaot-link-*` directory beside the output, runs `clang --driver-mode=g++
  --target=<triple>` (`--linker`, else PATH/`%ProgramFiles%/LLVM/bin`) with the runtime archive
  (`--runtime-library`, else the build's own path relative to `erlangaot`, checked member by member
  for arch/object format via LLVM Object), then replaces the output. Project builds (step 7) stage every executable target (manifest `output`/`entry`, CLI `-o`/`--entry`; planner `project/plan`) via `linking::stage_executable`, queue `PendingExecutable`s and `publish_executable` them only after all targets succeed; library targets compile in memory.

- Source printing (`docs/compile.md#source-printing`): frontend `print_source` prints parsed syntax as Erlang source
  (`--print-source`), with `SourceNotes` hooks for `Expression :: Text` annotations and comments above forms; it
  round-trips to the same tree (`printing_source`). `--print-types` = source + `%% inferred:` signatures + type
  annotations from `semantic::types::type_source` (graph types in Erlang type syntax).
- Term printing: runtime `format_term` renders `~w` or emulator display text iteratively under a
  byte cap (maps in map-key order); `erlang:display/1` is a body-only service writing to
  `RuntimeOptions::standard_output`. Goldens in `tests/fixtures/printing/`.

- Six end-goal program fixtures (`tests/fixtures/programs/`) carry OTP stdout/exit
  goldens and today's exact compile diagnostics; later steps update `compile.txt`
  until step 58 runs them as executables.

- End-to-end executable tests use the step-8 runner (`tests/compiler/executables/run.py`):
  a case directory is sources plus an OTP-generated `golden.json` (stdout, exit status, authored
  stderr regex, source hashes); CMake globs cases into `executables_<case>` and the runner links
  them per fast/full policy/driver matrix. Later feature steps add cases there.

- Remaining work is expanded in [plan 11](11-plan.md) as 78 small steps.
  The completed pattern/guard checklist is retired; its contracts/evidence remain
  in that plan and [the archive](00-finished.md#completed-patternmatch).
