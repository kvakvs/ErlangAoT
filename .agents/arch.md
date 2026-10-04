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
  operators and acyclic local/exported batch calls. Flat match plans carry explicit success/mismatch continuations and tentative
  SSA values. Each candidate owns fresh bindings; head/guard rejection advances with original arguments. All clause bodies feed call/inference/atom/inspection analysis. Result joins preserve only common argument relations. Checked equality is representation-aware; exhaustion
  raises function_clause. Body matches save the RHS once and reuse the matcher; only success publishes bindings, while badmatch retains the RHS and exits before later work. Unconditional heads retain direct argument projections inside their root scope.

- Ordinary record layouts retain declaration order, defaults and source provenance.
  Bounded per-use expansion reuses tuple matching and rooted construction. Checked
  access validates tag/arity; guard mismatch rejects, body badrecord owns its payload.
  Updates and native/qualified/inferred records retain separate capability owners.

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
  never intern on evaluation. Iterative structural order uses decoded integers, atom spelling, tuple arity/fields and cons heads/tails, with bounded work.
  Failed registration may retain valid atoms, but publishes no module/slots.

- Integers normalize target-sized values to immediates and store larger immutable
  sign/magnitude words in the indexed heap. Owned bounded multiprecision temporaries,
  explicit word codecs/carry/borrow and double-width LLVM fast paths preserve exact
  promotion/demotion. Numeric semantic errors reject guards; badarith/abs badarg
  and infrastructure failures retain their separate body/channel outcomes.

- Finite binary64 values use owned indexed storage and rooted literal services.
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
  Borrowed service arrays and both success outputs are rooted. Cells remain with heap backing until
  teardown and final host-pin release; GC/copying retain their separate owners.

- Stable heap chunks support bounded word allocation, aligned reservations, rollback
  and explicit resource destruction. No GC or graph copying runs. Revision-4
  generated scopes register arguments/temporaries, clear failed candidates, transfer
  result ownership before pop and restore entry depth after native exceptions.
  Exact-start object indices prove ownership before extraction. Compound host handles
  pin backing, deny expired access and retain returned children/error payloads across
  growth. Constructors publish initialized tuples/cons spines transactionally;
  metadata/backing allocation failure rolls back. Rooted runtime scratch buffers
  keep wide source constructors off the native stack.

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
  registers every module before entry, builds argv, runs the entry in one context and maps
  return/halt/exception/infrastructure outcomes to exit 0/N/1|127/70. `erlang:halt/0,1` records
  `CallError::halted` in the checked channel, so halts unwind like errors.
- Linking (`linking/`, LLVM-private): positional `-o` keeps objects in memory, stages them in a
  private `.erlangaot-link-*` directory beside the output, runs `clang --driver-mode=g++
  --target=<triple>` (`--linker`, else PATH/`%ProgramFiles%/LLVM/bin`) with the runtime archive
  (`--runtime-library`, else the build's own path relative to `erlangaot`, checked member by member
  for arch/object format via LLVM Object), then replaces the output. Project builds (step 7) stage every executable target (manifest `output`/`entry`, CLI `-o`/`--entry`; planner `project/plan`) via `linking::stage_executable`, queue `PendingExecutable`s and `publish_executable` them only after all targets succeed; library targets compile in memory.

- Term printing: runtime `format_term` renders `~w` or emulator display text iteratively under a
  byte cap (maps in map-key order); `erlang:display/1` is a body-only service writing to
  `RuntimeOptions::standard_output`. Goldens in `tests/fixtures/printing/`.

- Six end-goal program fixtures (`tests/fixtures/programs/`) carry OTP stdout/exit
  goldens and today's exact compile diagnostics; later steps update `compile.txt`
  until step 58 runs them as executables.

- Remaining work is expanded in [plan 11](11-plan.md) as 78 small steps.
  The completed pattern/guard checklist is retired; its contracts/evidence remain
  in that plan and [the archive](00-finished.md#completed-patternmatch).
