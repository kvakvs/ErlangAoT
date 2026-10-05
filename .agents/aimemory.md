# Project memory — 2026-10-03

Completed pattern/guard steps 1–20 and added15a are archived in .agents/00-finished.md.
The retired checklist was removed by user request; durable context is .agents/11-plan.md#completed-patternmatch.
No subagents authorized or used. Separate current commits: step17 7d83b99,
step18 684af35, step19 428c388; final step20 commit 2be9626.
Each implementation commit followed fresh combined Debug build/fullCTest/Lizard/tidy.

Current admitted scope: recursive local/exported remote functions, ordered heads,
body matches/sequences, grouped/strict/lazy guards, rooted checked construction/
access/comparison/numeric services for owned atoms, arbitrary integers, finite
floats, tuple/list/string/map/bitstring/ordinary tuple records. Descriptor ABI4,
checked failure channel2; ownership checked before extraction, handoff before pop.
Guards reject reached semantic errors; infrastructure failures halt. Specs grant
no representation authority. Clause candidates isolate SSA, facts index successful
whole-value assignments; extracted/unproved values stay top. Specialization/work/
IR caps unchanged; generic fallback verifies.

Evidence summary docs/validation.md (history), patterns.md and
guards.md; old step records/evidence JSON removed in plan11 step 1C (Git history). Nineteen owned corpora: 67,634 native expected outcomes +106
semantic rows, eight driver/policy combinations, two executions =>1,082,144
comparisons. Seeded closure1969, seed0x29A07, depth64,width255,128alternatives;
allmanifest hashes and81catalog mappings reconciled. 77signatures admitted-domain,
fourdependency gates:self0,node0/1,nativeis_record1. Positive pid/port/ref/fun
representations not admitted. All19 explicit OTP regeneration --check pass.
Updated example42,-7,record,map,binary,list,integer,other passes four policies.

Step20 final fresh gate124/124 no skips,167.43s,258production quality units,
LizardCCN10/tidy cognition10 unchanged. Logs build/patternmatch-step20/{gate,tests,
quality,all-regeneration}.log. Initial123/124 failed testserialization256 around
wrapped255-celllist; boundedtesttransport512 fixes; no production ceiling changes.
Historical records preserved. Native Windowsx64 only; Linux/AppleSilicon/native32
and new frontend sanitizer unavailable. Foreignobjects/32-64IR/suiteparsing separate.

Official maint-29 fetched at each OTP-dependent step; last2026-10-03 unchanged
21776803ecd11f5fa948732c0ec66b8f325dedfc upstream/pin/cleancheckout. Follow
docs/otp-reference.md to refresh future work, synchronize hashes/grammar/docs and
preserve historical revisions. Oracle29.1.1/ERTS17.1. Installed escript broken
erl.ini; workspace shim build/patternmatch-step16/otp-launch/escript.exe points
installedruntime via privateerl.ini, no installedfilesmodified. Regenerate
python tests/compiler/patternmatch/regenerate.py --otp references/otp --escript
build/patternmatch-step16/otp-launch/escript.exe --corpus all --check.
Normalbuild/test OTP-free; liveaudits opt-in. Fixtures writebytes UTF8LF (-text).

Toolchain LLVM23.1.2/SDK, VS18x64SDK10.0.26100,/MT iterator0, Ninja, Boost1.90,
toml3.4,Lizard1.24,tidy22.1.8. VSdevshell required for native consumers; SDKdependent
gate and protected .agents/.git writes require escalation. No auto-review rejection.
Gatecmd build/patternmatch-step20/gate.cmd uses --fresh combined Debug. Keep intent
comments/clangformat and low complexity; no quality suppressions/threshold increases.

Remaining owners: F01launcher/linking,F04GC,F05graphcopy,F07identities,F16other
control,F17updates/record_info/native-qualified-inferred,F18closures,F19dynamiccalls,
F20handlers/raise/traces,F21recursion/tailcalls,F22-25processworkers/messages/receive,
F26genericproductionbuiltinregistration,V01nativematrix,V02sanitizers. Backlog closes
only delivered function/body/guard and prerequisite representation/root/service slices.
OTP loader huge literalrecordguard and core_to_ssa legacy-modernimport crashes
remain documented17/18; do not claim those contexts as executed OTPgoldens.

Planning update2026-10-03 (rewrite): user deleted 51-step draft; .agents/11-plan.md now
78 small single-commit steps, phases A-N, each with success criteria+tests. Exes early
(3-8) so later steps test via executable golden runner (step8). Decisions: 3 entry,17 frame
model,24 GC policy,53 ports,71-77 D-items. Mandatory1-70+78. 01-todo links remapped.

OTP source cleanup2026-10-03: replaced130 notice-bearing fixture source files and
removed copied assertion/file headers+license. Local source fixtures now under
tests/fixtures/patternmatch/fragments (222 Erlang/include files+corpus.json).
authored.py stages fixed local inputs; stored.py resolves source/observation roots.
Regeneration observes local code, never extracts OTP source. Keep observations
committed per explicit user answer; OTP source/generated headers/audits transient.
erlfmt installed+moved to ignored thirdparty/tools/erlfmt per user instruction;
valid source/header and changed term files formatted. Preserve user's AGENTS edits.
Fresh combined gate125/125+258unit Lizard/tidy, all19 --check, upstream audit,
grammar/corpus pass; maint29+pin unchanged21776803. Audit summary docs/validation.md;
logs build/otp-cleanup*. Existing dated validation evidence not rewritten.

MSVC cl (non-clang-cl) 2026-10-03: /external:W0 misses codegen C4702 (Boost.Parser) and STL pair
narrowing C4244/C4267 from LLVM headers; disabled only on erlang_aot_parser_dependency and
erlang_llvm_sdk interfaces for cl. erlang_aot builds under cl; runtime still fails cl C4554
(float_factory.cpp/bit_factory.cpp:23, project code, already parenthesized).

Plan11 steps 1-8I done 2026-10-03..04 (compact record in .agents/11-plan.md; logs build/plan11-step*).
Step facts beyond the plan record:
- 1/1A/1B: maint29 21776803 unchanged. Fast mode = matrix.py O0 positional + O2-off project, mutations
  once, LABELS full_only excluded. ERLANG_AOT_QUALITY_BASE overrides HEAD for changed-scope quality.
- 2: fixture layout project.toml, src, fixture.json entry+argv, expected/{stdout.txt,golden.json},
  compile.txt exact stderr; oracle tests/compiler/programs/oracle.escript (main/1 in spawn_monitor).
- 3/3A: project/entry parse_entry, manifest decode entry(); driver/entry resolve_entry after
  index_inputs; escript rewrites line 1 to `-module('<base>__escript').`, %%! warns.
- 4: OTP display = C printer erl_printf_term.c, not ~w. OTP 26+ map order follows atom index (varies per
  VM run) / hash order > 32 keys -> print key order, goldens skip unstable (values.stable_order).
  AtomStorage::boolean needs "true" pre-interned (registration does it).
- 5: startup llvm module appended after inputs (index >= inputs = startup). Windows argv via
  _configure_wide_argv + __wargv (no shell32). halt -> CallFailure{halted, halt_status, slogan}.
- 6/6A/7: linking lib erlang_linking; default runtime = ERLANG_AOT_DEFAULT_RUNTIME relative to
  erlangaot; clang found via --linker, PATH, $ProgramFiles/LLVM/bin; works outside vcvars (lld-link).
  Project rule: link iff not frontend action AND (manifest output|entry or CLI -o|--entry); else
  in-memory library compile. -Os = O2 pipeline + optsize attr (LLVM 23 has no Os level).
- 8: run.py uses matrix.combinations() (8 full / 2 fast), ~3 s per case. Pass --suffix=... as one token.
- Phase C (user commit bb09359): single heap only, old heap/minor GC deferred (F04).
- 8B: refc cell linked only after publish (built with empty shared_ptr, rollback needs no destructor).
- 8C: abi::v1::primary_mask is 32-bit `unsigned`: cast to Word before `~`.
- 8E: Term {value_, atom_, heap_, weak lifetime_, collections_}; GeneratedCallState::visit rebinds payload.
- 8F: Segment = ONE operator-new of 4096 - 2*sizeof(void*) (Win64 HEAP_ENTRY 16 B). Segments exist only
  because generated code holds absolute Word* from roots_enter across calls.
- 8G: HeapArea{words_,capacity_,top_}; HeapOptions positional {words, bytes}; rollback drops a new
  fragment/heap block, so tests expecting capacity 0 after failure hold. Fragment vector grows
  geometrically (reserve(size+1) per push was quadratic). 100k kernel ~3,000 fragments.
- 8H: Copier ctor allocates to-space then ++collections_ (GeneratedCallState::visit rebinds payload with the
  new count). First block = policy(used words); shrink = second full copy when <25% live (no offsetting).
  collect() never touches the generated failure channel; native/failure consumers now use deferred send
  and heap reservation faults. Term::bit_slice is unimplemented (link error): slice via erlang_aot_bits_v1.
  Python Path.read_text defaults to cp1252 here: always read_text(encoding="utf-8") or use Edit.
- 9: case = one-input body plan per clause (`body_pattern_plan`, `semantic::pattern_root`); exported names share one
  BindingId across clauses (first clause allocates, later reuse via branch_names stack), so lowering PHIs only
  `Function::exports`; inference keeps multi-definition identities top. expression_children(case) = scrutinee +
  guard tests + bodies (patterns via function.patterns); guard_analysis schedules case guards as guards.
  Pre-existing fix: capability pattern planning skipped after failed binding pass (was "invalid map<K, T> key").
  Bindings corpus: 8 case_* rows, OTP classes via regenerate --corpus bindings; sibling_local now compiles.
  Old step-9 attempt (pre phase C) sources reused for executables case_select/case_scope.
- 10: if = case without scrutinee/pattern via semantic::branch_clauses (Branch{pattern*, guard*, body*}); CaseJoin.value
  null for if. Runtime erlang_aot_raise_v2 whitelists atom-only reasons (plain_reason) -> new plain reasons need it.
  OTP lint rejects `X =:= 0.0` in fixtures (match_float_zero). bindings.term rows are authored expectations; OTP
  regenerate only verifies them (fill corpus.json row sha256 = LF digest). Edit tool may write CRLF into .py: normalize.
- 11: raise = ErrorReason raised_error/exit/throw (11-13) via existing erlang_aot_raise_v2 (no new symbol/ABI rev);
  payload_reason range covers them; startup exception_class(). body_builtin moved to pattern_calls.cpp (needs
  auto_import); unqualified only for the raise family, display/halt stay erlang:-qualified until step 36. Oracle stderr
  prints `uncaught <class>: ~p` (compare class/reason manually; not stored).
- 12: catch = ExpressionLowering::handler (innermost catch); failure_exit and raise_reason branch there; cached
  bad_argument/bad_arithmetic exits reset inside the catch. Runtime erlang_aot_catch_v1 in process/exceptions
  (error_name shared with startup); stack placeholder []. binding_children lacked CatchExpression (semantic walks
  skipped catch bodies: segfault / "invalid map<K, T> key") - fixed. OTP 29 warns deprecated_catch by default and
  the oracle uses warnings_as_errors: fixtures need -compile(nowarn_deprecated_catch); nowarn_* options admitted.
  Program fixtures hide stacks with show({'EXIT',{R,S}}) when is_list(S).
- 13: CatchClause class/stacktrace are ExprIds (parser make()). Branch::handler + first_handler: try = of clauses then
  catch clauses. Walker ProtectedScope (catches map) protects only the body; handlers() takes {class, reason} via
  erlang_aot_exception_v1, catch clauses start from pre-try bindings; unmatched -> erlang_aot_reraise_v1 (raised_*
  reason, observably identical). Walk::visit split (visit/branch) for Lizard CCN. Bindings OTP regenerate: use
  "C:/Program Files/Erlang OTP/erts-17.1/bin/escript.exe" directly (otp-launch shim gone). Atoms > numbers in term order:
  `zero > 0` is true in guards.
- 14: after = second ProtectedScope map `afters` + `after_paths` (result/resume/exception); after body lowered twice
  (normal path, then from the after handler). Never return/move ProtectedScope by value: MSVC std::map move may throw ->
  tidy bugprone-exception-escape; emplace in place, extract node handles. `opt->` and `.value()` on std::optional are
  both flagged by bugprone-unchecked-optional-access: test first. Process budget overflow reports resource_limit (7).
  `after` is a reserved word: fixture atoms must not be `after`.
- 15: trace captured at GeneratedCallState::fail from GeneratedRoots frames (FrameDescriptor per function, private
  `frame.<symbol>` global); OTP compiler turns calls to never-returning functions into tail calls, so trace fixtures
  need functions that can also return and non-tail call sites ({tag, f(X)}). Term::is_function is declared but not
  defined (link error). Recursion admitted from step 18.
- 16: maybe keeps ast::MaybeMatch (not an ExprId): walkers use semantic::maybe_operands and Visit.field = item index.
  The "pattern matching" capability fallback in match_plan is unreachable from source (all PatternKinds planned).
- 17: decision only (docs/execution-model.md). Prototype tests/prototypes/execution_model/run.py uses clang++ from
  PATH or %ProgramFiles%/LLVM (works outside vcvars); generated.cpp is freestanding (-ffreestanding, stddef/stdint only)
  so it cross-compiles for all 7 targets. ARM asm has no TAILCALL comment: run.py counts b/br/bx to symbol/register.
  clang-format collapses one-line function bodies there (root .clang-format). Step 19 must: split walker at calls via
  body resume switch, reload frame base after every transfer/service that can push, replace GeneratedRoots + handoffs
  with flat stack + x registers, entry ABI becomes void(Process*).
- 18: CallGraph::components (Tarjan, callees first; members sorted by declaration index) + flattened order. Inference
  solve(): members start Fact{bottom}; merged() treats {bottom, no argument} as identity (also in clause joins);
  expression facts erased per round via `recorded` (stale earlier-round facts would be unsound); 16 rounds then
  graph.exhausted() (widened flag). A 16-function ring hits the limit, 15 converges. Codegen needed no change (all
  functions declared before definition; native recursion). Catalog recursive_calls implemented (placeholders.py set must
  equal deferred compiler entries). type_inspection.py needs an absolute tool path when run by hand.
User directions (keep):
- All ABI symbol/namespace versions collapse to v1 in plan step 78A (never released; no compatibility).
- Minimal first, iterate later; no defenses for impossible cases (8D: no start bitmap / interior-pointer
  checks, classic ERTS trust model). 8F first version (doubling/spare/trim) rejected as over-engineered.
- ERTS host model: no handle table; Term valid only in own process, read-only elsewhere.
- Binaries > 64 B stay std::shared_ptr buffers outside heaps; prefer C++ style designs.
- Heap is one block per process; each process owns its heap.
- Don't stage user's test1.toml. Stale IDE buffers may revert plan edits: re-check before commit.
Host and tool gotchas:
- VS discovery (see AGENTS.md Windows Toolchain): vswhere at C:/Program Files (x86)/Microsoft Visual Studio/Installer/
  vswhere.exe -latest -products * -property installationPath -> C:/Program Files/Microsoft Visual Studio/18/Community;
  vcvars64 = <that>/VC/Auxiliary/Build/vcvars64.bat. Copy build/plan11-step11/*.cmd (sed step dir) for new steps.
- Gate: vcvars64 + PATH "C:\Program Files\LLVM\bin"; never pass LLVM_DIR (skips /MT+IDL0). Scripts
  build/plan11-step8g/{gate,rt,quality,all}.cmd; cmd /c needs full .bat path; run .exe via PowerShell.
- build/debug may have BUILD_TESTING=OFF and Ninja may not rerun CMake: use the fresh gate.
- First run after runtime source edits can time out tests while native sub-builds recompile; rerun.
- clang-tidy may crash (0xC0000005/0xC0000409) or exit 1 silently with 2 jobs; rerun with one job.
  The check targets do not forward QUALITY_JOBS: run `cmake -DQUALITY_SCOPE=all
  -DQUALITY_BUILD_DIR=<build> -DQUALITY_NINJA=<ninja> -DQUALITY_JOBS=1 -P cmake/CheckClangTidy.cmake`
  (script build/plan11-step8i/rerun.cmd). codegen_dependency can time out (120 s) under full -j 16. Runtime CMake edits select all tidy units.
- MSVC C4554 false positive on static_cast<Word>(n - 1) << shift: hoist to a local.
- Bash heredocs mangle non-ASCII, `\n` and `\`, and many quotes break them: write files with Write
  or scratchpad scripts. `cat > file` without heredoc hangs. Python Path.write_text writes CRLF on
  Windows: use write_bytes. CMake drops empty ARGN args; execute_process needs ENCODING UTF-8.
- ADL can pick std::quoted for a local `quoted`: rename. No make here; gmake at C:/Strawberry/c/bin.
- OTP oracle: erts-17.1/bin/escript.exe directly (bin/escript.exe segfaults). erlfmt: escript with
  code:add_path("thirdparty/tools/erlfmt/_build/local") + erlfmt:format_file(F, []).
