# Project memory — 2026-10-03

Completed pattern/guard steps 1–20 and added15a are archived in .agents/00-finished.md.
The retired checklist was removed by user request; durable context is .agents/11-plan.md#completed-patternmatch.
No subagents authorized or used. Separate current commits: step17 7d83b99,
step18 684af35, step19 428c388; final step20 commit 2be9626.
Each implementation commit followed fresh combined Debug build/fullCTest/Lizard/tidy.

Current admitted scope: acyclic local/exported remote functions, ordered heads,
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

Plan11 step1 done 2026-10-03 (uncommitted): maint29 unchanged 21776803; fresh clang-cl Debug
125/125 serial 729s, check-quality 258 units pass (tidy 22.1.8 from .venv-quality), 14 audit-only
tests pass (build/plan11-audits), regenerate --check all 19 match. Gate cmd: vcvars64 + PATH
"C:\Program Files\LLVM\bin"; DON'T pass LLVM_DIR (skips /MT+IDL0 -> probe mismatch). Logs
build/plan11-step1. User added plan steps 1A (fast/full test split) and 1B (changed-file quality).

Plan11 step1A done 2026-10-03 (uncommitted): ERLANG_AOT_TEST_MODE fast|full (unset=full).
tests/compiler/patternmatch/matrix.py combinations()/option_lists(): fast = O0 positional +
O2-off project. mutations.cmake fast=1 pass. LABELS full_only: parser_consumer,
codegen_dependency, codegen_measurements. Presets debug-fast/windows-debug-fast (jobs 0),
make test (fast, TEST_JOBS=nproc), make test-full, make-test.bat TEST_MODE/TEST_JOBS.
Fast 122 tests ~60s wall (-j16 53s); full -j16 125/125 85s. Plan gate now fast per step,
full per phase. No make on this host; gmake at C:/Strawberry/c/bin/gmake.exe for dry runs.

Plan11 step1B 2026-10-03: changed-scope quality. cmake/quality_scope.py (+QualityScope.cmake)
selects lizard files and tidy units (ninja -t deps for header dependents; cmake/,.clang-tidy,
production CMakeLists -> all). Targets check-quality (changed) / check-quality-all;
ERLANG_AOT_QUALITY_BASE overrides HEAD. make format/make-format.bat changed by default,
format-all / FORMAT_SCOPE=all. First tidy run exited 1 with no diagnostics (silent analyzer
crash, known flake); unchanged rerun passed. mutations.cmake per-call TIMEOUT 5->30 (load stall).
cmd /c needs full path to repo .bat files on this host.

Plan11 step1C 2026-10-03: docs/ consolidated 81 files (5,370 lines + ~500 KB evidence JSON)
into 15 brief notes indexed by docs/README.md: preprocessor, parser, projects, compile,
semantic, specialization, abi, features, patterns, guards, terms, runtime, otp-reference,
validation (baseline, gate, provenance, test design, platform gaps, condensed history table).
Old step validation md/json, compile-tests.txt and per-topic pattern/runtime docs deleted;
originals in Git at 2777c98. README intro rewritten; all repo links repointed. Keep docs
current-state only; step logs go to .agents, not docs.

Plan11 step2 2026-10-03: six original fixtures tests/fixtures/programs/{textstats,frames,avltree,
ring,kvstore,supervise} (project.toml, src, fixture.json entry+argv, expected/{stdout.txt,golden.json},
compile.txt exact stderr with <fixture> paths). README = feature map (fixture -> steps). Oracle
tests/compiler/programs/oracle.escript runs main/1 in spawn_monitor, logger -> stderr, halt 0/1;
use erts-17.1/bin/escript.exe directly (bin/escript.exe segfaults; old otp-launch shim gone).
regenerate.py [--check]; programs.py <tool> <work> [--update-diagnostics] = CTest programs_compile;
programs_oracle opt-in. --check reproduced 3x. erlfmt CLI lacks getopt: format via escript calling
erlfmt:format_file/2 with code path thirdparty/tools/erlfmt/_build/local (compiled erlfmt_cli there).
`++`/`--` were ownerless ([arithmetic] notimpl) -> added to step 37. Logs build/plan11-step2.

Plan11 step3 2026-10-03: entry contract docs/executables.md. project/entry parse_entry (u32 names,
1..255, no ctrl/':'), SelectedEntry{name, origin}; manifest `entry` key (decode.cpp entry()),
plan entry_selection (CLI overrides, single target else exit 2). driver/entry resolve_entry after
index_inputs in analyze (EntryRequest{selected, required=-o}); Analysis.entry kept for step5.
Exit: return/halt()=0, halt(N)=N, escaping exception incl exit(normal)=1, runtime failure=70.
Bash tool mangles non-ASCII and `\n` in heredoc python; use Edit/Write for such text.
CMake execute_process needs ENCODING UTF-8 for UTF-8 stderr matching on Windows.
Plan11 step3A 2026-10-03: escript auto-detect by "#!" line 1 (no CLI flag). driver/escript
escript_source rewrites line1 -> `-module('<basename .->_>__escript').` unless first form is
-module (lexical scan), %%! line 2/3 -> warning. CompilationInput.escript -> semantic::index(...,
escript) -> semantic/escript index_escript (main/1 required+exported, -mode validated);
capabilities allow -mode only in escripts. ResolvedEntry.escript -> exit 127 later (step 5).
Name clash: ADL picked std::quoted for local `quoted` -> renamed atom_literal.
Plan11 step4 2026-10-03: term printing. runtime output.hpp (TermStyle write|display, format_term,
OutputSink, write_output); terms/term_text{,_scalars}.cpp; builtins/output.cpp erlang_aot_display_v1
(abi/output.hpp, Status::output_failure=13). Compiler: body_builtin() erlang:display/1 only (qualified),
ImmediateOperation::display marker -> lower_display. OTP display = C printer erl_printf_term.c (%.6e
floats, printable latin1 lists as strings, <<"ascii">>, no '@'/reserved quoting), NOT ~w. OTP 26+ map
internal order: atom keys by atom INDEX (varies per VM run!), >32 keys hash order -> we print map-key
order (=~kw ordered); display goldens skip by structural rule (values.stable_order). Goldens
tests/fixtures/printing (regenerate.py --escript erts-17.1/bin/escript.exe [--check]); match_wire.hpp
reused. AtomStorage::boolean needs "true" pre-interned (registration does it). Python write_text on
Windows writes CRLF: use write_bytes/newline=''. erlfmt: build/plan11-step4/fmt.escript (format_file(F,[])).
Step4 gate: fresh fast 128/128 (first run after runtime source changes timed out 9 tests while ~10
native sub-builds recompiled the runtime; rerun clean, 55 s). check-quality tidy with 2 jobs crashed
clang-tidy (0xC0000005/0xC0000409) on random unchanged units 3x; same official script with
-DQUALITY_JOBS=1 passed all 265 units (build/plan11-step4/tidy1.cmd). Runtime CMake edits select all units.
Plan11 step5 2026-10-04: startup object. abi/startup.hpp StartupDescriptor{abi,bits,modules**,count,
entry module/function bytes,flags(escript=1)}; erlang_aot_main_v1(int,char**,const void*) +
erlang_aot_halt_v1(ctx,word) (runtime/src/startup/{startup,arguments,halt}.cpp). Codegen
codegen/startup.cpp appends llvm module "startup" AFTER inputs (serialize/emission/progress treat
index>=inputs as startup; OutputBuffer.startup -> artifact eav1_start). Set from Analysis.entry in
driver/backend.cpp (explicit entry only; -o still notimpl until step 6). Windows argv via
_configure_wide_argv(_crt_argv_unexpanded_arguments)+__wargv (no shell32). halt -> CallFailure
{code=halted, halt_status, value=slogan}; body_builtin erlang:halt/0,1 qualified only. Mangling
verified with clang --target for 7 triples (decl file build/plan11-step5/mangle). CMake ARGN drops
empty args: test "" via direct execute_process. Consumer exe from objects only needs LINKER_LANGUAGE CXX.
Manual link: clang-cl /MT obj\*.obj build\debug\lib\erlang_runtime.lib (vcvars). Logs build/plan11-step5.
Plan11 step6 2026-10-04: positional -o links. compiler/src/linking/{link,toolchain,runtime_library}
(lib erlang_linking, LLVM Support/Object private). FrontendRequest.executable_output (optional path,
replaced bool). Options --linker/--runtime-library (require --output). Default runtime path =
compile def ERLANG_AOT_DEFAULT_RUNTIME ($<PATH:RELATIVE_PATH> runtime file vs erlangaot dir).
Clang: --linker, PATH clang++/clang, then $ProgramFiles/LLVM/bin; args --driver-mode=g++ --target.
Works outside vcvars (clang finds MSVC; picked lld-link). .exe appended for Windows targets w/o ext.
Project -o still notimpl (step 7). Tests: linking_executable (other archive = erlang_artifacts lib
for undefined-symbol link error); entry/escript/cli tests stop at --runtime-library absent.lib.
examples/compile/client.erl gained main/1. Bash heredoc drops `\` -> use Edit for regex escapes.
cmd: run .exe in cwd needs PowerShell (cmd said not recognized). Logs build/plan11-step6.
Plan11 step6A 2026-10-04 (user request): --project + explicit -o (already single-target only) links;
backend.cpp notimpl branch removed, FeatureId::executable_linking status implemented (failure_test
linking_executable). Step 7 = manifest `output` without -o + multi-target. User's earlier amend of
step 6 reverted the plan section (stale IDE buffer); restored. Don't stage user's test1.toml.
Plan11 step7 2026-10-04: project builds link. Rule: target links iff not frontend(check/print/emit/inspect/
print-types) AND (manifest output|entry or CLI -o|--entry) -> project/plan executable(); else in-memory
library compile (kept so library-target compile checks work). linking: StagingDirectory (movable),
StagedExecutable, stage_executable/publish_executable/link_executable; LinkRequest.create_directory for
manifest outputs (not -o). Driver PendingExecutable{target, executable} queue in project_backend; publish
artifacts then executables; colliding() catches names equal only after .exe. links_project() allows
--linker/--runtime-library without -o for --project (not --new-project). Test linking_project.
Ninja did not rerun CMake after tests CMakeLists edit (cache had BUILD_TESTING=OFF): use the fresh gate.
-Os: LLVM 23 has no OptimizationLevel::Os/Oz; size = O2 pipeline + optsize fn attr (as Clang). LinkRequest.strip_unused
adds --gc-sections / -dead_strip / /OPT:REF,ICF. MSVC link.exe already OPT:REF without /DEBUG, so Os==O2 size there.
build/debug: BUILD_TESTING=OFF, needs vcvars64 env (LIB) for runtime/dep builds; MSVC C4554 false positive on
static_cast<Word>(n - 1) << shift (hoist to a local).
Plan11 step8 2026-10-04: executable golden runner tests/compiler/executables/{cases,run,selfcheck,regenerate}.py;
cases tests/fixtures/executables/<case>/{*.erl,golden.json} (authored entry/runs[].args/stderr regex/sources;
OTP exit_status/stdout/oracle_version/reference/inputs). CMake glob CONFIGURE_DEPENDS -> executables_<case>;
selfcheck (wrong golden diff, stale hash); executables_oracle opt-in. Oracle = programs/oracle.escript via
erts-17.1/bin/escript.exe. run.py uses matrix.combinations() (8 full / 2 fast), ~3 s per case on host.
Pass --suffix=... as single token (CMake drops empty args). erlfmt: escript with code:add_path(
"thirdparty/tools/erlfmt/_build/local") + erlfmt:format_file(F,[]) (beams flat there). Logs build/plan11-step8.
Plan11 phase C inserted 2026-10-04 (user commit bb09359 "Replan new heap"): steps 8A-8I classic ERTS
heap before step 9; single heap only (old heap/minor GC deferred, F04 backlog item). User direction:
binaries >64 B stay std::shared_ptr buffers outside all heaps; refc_binary cell holds the shared_ptr
+ off-heap list link, relocated by move-constructing that member. 8C note from user: prefer C++ style.
Plan11 step8A 2026-10-04: docs/runtime-heap.md contract; full_only CTest runtime_heap_measurements
(tests/runtime/heap_measurements.cpp, counting operator new). Baseline bb09359 x64 Debug: 100k kernel
264/81 ms, 700000 used words, side bytes 24 MB (~80 B/cell std::map index), 66 KB/context.
Logs build/plan11-step8a.
