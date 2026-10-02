# Current checkpoint 2026-10-02

Step14 committed416dfd3; step15 maps committedde9c255. User-addedstep15a fixtures READYCOMMIT:14corpora197files49,959nativeexpectations+95patternacceptance, ~9MB. FreshOTP-free118/11898.86s andfull244qualitypass. The 118-testgate used OTP_AUDITS=OFF and deliberately nonexistentESCRIPT/OTP_SOURCE_ROOT. Lateraudit-configscript inadvertentlyalsofreshconfigureddebugwithrealOTPpaths (auditsstillOFF); pathsunusednormaltests. Forfutureenvscripts copyONLYfirst5lines,not6 (line6isalreadyacommand). Optional132-testinventoryconfigurepassed build/otp-audit-check;directupstream.py passed allcatalogs/suites/oldseed+regenerate--check14. Codegenroutineallstoredfixtureload; originalfunctions retainedsolelyregeneration, newregenerate_cases supportsbindings/patterns/baseline/differential. UserAGENTS.md addedOTP-freegoldenpolicy, preserve asprovided andincludeinrelevantfixturecommit. Newregeneratecorpusfuturefeatures explicitlythencommitdata; normaltestsNEVERregenerateexpected. Auditregeneratorwith--check writesignoredtempdirs only. Newfeature16updatepatterns.json+manifest+explicit regenerate patterns snapshots forcapabilitychanges.

Next16bitstrings,17records,18remainingguards,19inferenceoptimization,20finalvalidation; userrequestedcontinueallstepscommittingeach. Noagentsauthorized. Allprevious15notesbelowhistorical. Scripts step15a gate freshcombinedOFF;copyforfuture normalgate. NeedexplicitOTP audit/regeneration source21776803ecd11f5fa948732c0ec66b8f325dedfc checkedmaint29at taskstartunchanged. DoNOToverwritesource duringnativeCTest/quality. Fullquality~minutesmandatoryeachcommit. No needresetfiles; reviewerrejectedunnecessaryHEADresetbefore15 but safelycompletedviaselectivestaging/backups. Current15aonlytest/build/docs files, productionunchanged.

# Current checkpoint 2026-10-02

Step14 committed416dfd3. Step15 maps passed fresh131/131286.68s +244productionquality; commit required next. User added separate step15a: project-owned OTP generated inputs/results; OTP must NOT be required for building/testing (user modifiedAGENTS.md, preserve and exclude map commit). 15a currentlydraft filesstored.py/regenerate.py +generated10corpora49,803results about9MB; frozen native test/CMake versions backed up inbuild/patternmatch-step15/fixture-update. Tests temporarilyreturned toliveOTP fordistinctmapcommit;8oldfiles mayhaveformatting-onlydiff, exclude frommapcommit. DoNOTgitreset/restore; autoreviewrejectedHEADreset8testfiles toavoidlosingfixturework. Safe backups exist but noresetneeded. Freeze step15a must also address generalconfigure+parser/semantic OTPdependencies, notjustnative10. No subagentsauthorized. Useroriginalfinishthrough20continueafter15a.

# Current task checkpoint (2026-10-02)

User resumed and twice requested completing .agents/10-patternmatch.md through step20, committing each step. Latest instruction supersedes previous stop-at13. Step14 is validated; commit title `[compiler] Implement floats, mixed comparisons and numeric guards (F11/F12)`. NEXT step15 maps, then16 bitstrings,17 records,18 guards,19 optimization,20 final validation. No subagents authorized. Source branch started clean at c105939. No user changes to preserve.

Step14: fresh combined 129/129 CTests; 14,436 float OTP/native calls each four policies; full Lizard and238production tidy pass. Logs build/patternmatch-step14/{gate,tests,quality}.log; docs step14 validation/evidence andfloatcontract. Upstream maint29 fetchedOct2 unchanged21776803ecd11f5fa948732c0ec66b8f325dedfc, checkoutclean. Oracle29.1.1/ERTS17.1. WindowsLLVM23.1.2/tidy22.1.8/Lizard1.24. Float layoutheader+8bytes inWordarray memcpy; no pointer deref beforeownercheck. New ABI floats.hpp8networkbyte service rootedslots. NumericOps appended afterabsolute:divide,to_float,round,trunc,floor,ceil. PriorIDs preserved. `+ - *` integerfastpath retained, mixedfallback; unaryplus -0.0 -> +0.0 perOTP. Signedzero exactunequal/numericequal; mixedorder exacttruncateddoubleinteger+fraction, neverinteger->double. Integer->double exact53bittiestoeven, double->integerfrexpmantissashifts; range errors semantic (bodybadarith vsconversion/absbadarg), guardfailure andinfrastructureseparate. Structuralequality delegatesnumericorder. Floattestwire f16hex; hostnonfinite/owner/growth/expiry/malformedliteral+ordinalallocationcleanup. Generatedfaults floatguard/body reuseexistingseam.

IMPORTANT don't edit production headers/sources during tests orquality: separate nativeconsumers rebuildruntime fromsource, mismatchedoldcompiler breaksops. Firstpreliminary14teststoppedbecauseeditedenumwhiletests; finalgatecleanmatchingpassed. tools.write_stdin for execsessions; logsredirect. Scripts14build.cmd freshconfig;14incremental.cmd buildonly;14test.cmd fullctest;14focused.cmd quotesregexinsidebatch. PowerShellargs pipe regex passedthroughcmd unquoted causespipeline! Putquotedregexinscript. Gitmetadata/.agents/nativeVS needrequire_escalated. Pythonread/writetext explicitUTF8; fixtureswrite_bytesLF. Fullqualityfreshbeforeeverycommit. No threshold/suppressionchanges.

Step15 reading: maps reservedhost APIs map_size/map_contains/map_find/map_entries, with_map_entry etc in runtimecanonicalterms.hpp; mapfactory currentlyunavailable in terms/factory.cpp. NormalizedPattern storesASTmapchildren key/value; pattern_analysis alreadyenforcesexactfields/readincoming scope; keybindings semantics done. MatchPlanner mapcurrentlyrejected. Add MatchNodes mapshape/lookup withkeyExprId; lower_body(key) understate.rejection=mismatch; rootoutputs. Capability/service resolutioncurrentlyscansguards+bodies, notheadkeyexpr; needanalyzekey/sizeembeddedexpressions forservices andcapability. Calls supportsservice side table; no ad hocbindingbyname. Structural_order currentlypairworklist+globalexact: maps require keysalwaysEXACT (integersbeforefloats) and values ordinary/exact dependingcontext, so per-task exactflagneeded; compare map size,allorderedkeys,thenvalues. Mapimmutablecanonical sortedentries factory can stageallupdates beforeonepublication; avoid publishingpartialmapperfield. Need boundedlookup/sorting/comparison, preserve duplicatesvalueconstraints andinsertionindependence. ErrorReason currentlybadarith=5; addbadmap,badkey withrootedpayloadtransport inprocess/generated_calls.cpp; currently payloadonlybadmatch/badarg_value. Ordinarysource mapupdateallkey/val evalorder before mapvalidation matchesOTP mustprobe. Map pattern key failures mismatch notinfrastructure. Readplanfull step15beforecoding.

# Working memory — 2026-10-02

Current request: finish ALL .agents/10-patternmatch.md steps through20, commitEACHcompletedstep afterfreshcombinedconfigure/build/fullCTests/fullLizard+tidy; noagentsauthorized; noearlypartialfinal. Steps9 817a154,10 4b94f20,11 1fb7a48,12 2db37d2 committed. Step13nowcompleteREADYCOMMIT:127/127145.19s freshcombined;full231-unitLizard/tidy passed;16065OTPnativecalls4policies,bothCLImodes,7foreignobjects;3unchangedOTPhelpers+seed29013 256signedpairs<=400bits. Wordcanonicalsign/magnitude, integer_{factory,access,values,decimal,words,sum,operations,service,literal}; exactmillionbitbudget/10kdecimaltext; explicitunsignedcarryborrow/decimalaccumulation, eagerBoosttemporaryownership; rootedbigliteralABI; LLVMdouble-widthchecked+/−/* &SSAUpdater joins; correctbadarithvsabsbadarg; largearity/elementchecking preservesinfrastructure. Lifecycleglobalnew sweeps construction+arithmetic; formatterstdostringstreamuses2MSVCfirst-uselocalefacets,testwarmsthosebeforeperintegerleakcount; sweepthenbalancesallordinals. Qualityfindingsresolvedwithoutthreshold/suppression changes. Logsbuild/patternmatch-step13/{gate-complete,tests,quality}.log. docsvalidation/evidence/contract andagentsplanarchfilesupdated. NEXTcommit13then14–20. Step14DRAFTSbuild/patternmatch-step14notcompiled/admitted: floats.hpp,float_factory.cpp,numeric_conversions.cpp,numeric_order.cpp,float_operations.cpp,numeric_service.cpp,float_literal.cpp,floats.py; design-notes &OTPprobe. Numeric conversiondraftusesexact53-bitmantissaties-even forinteger->double andexactfrexpdouble->integer. Verifybeforecopy. FloatsABIcharbytes8networkIEEE with verified MSVC64?erlang_aot_float_v1@@YAEPEAXPEBD_KPEA_K@Z,MSVC32?erlang_aot_float_v1@@YAEPAXPBDIPAI@Z,Unix64_Z19erlang_aot_float_v1PvPKcmPm,Unix32_Z19erlang_aot_float_v1PvPKcjPj. Needheaders/enums/friendCMake/numericdispatcher/capability/lowering/matchplan/comparison+wiref16hex+wiring/testsdocsallgates. ProbeOTP29signedzeroexactdifferent,numericequal;min/maxfirstonties;hugeinteger/hugeintegerbadarith;floattoo-bigbadarg;roundtiesawayfromzero. Preservepinmaint29 21776803ecd11f5fa948732c0ec66b8f325dedfc taskstartchecked;oracle29.1.1/ERTS17.1,SDK23.1.2,qualityLizard1.24/tidy22.1.8. .agents/.gitwrites/nativeVS18scripts escalation; build.cmd includesFRESHconfigure—isolatedprobescopyONLYfirst5envlines. ALL Python filesystemtext encoding=utf8. DoNOTcopyold13draftsoverproduction.




- Patternmatch step 5 complete (2026-10-02): flat Function::patterns preserves
  original/ungrouped expression IDs, kinds/children and owned scalar constants.
  bind_pattern overload handles RestrictedPattern/PatternCandidate; body '=' remains
  RHS-first and pattern '=' is alias constraints. Read scopes contain incoming names
  only, except binary-local preceding segments. Never publish sibling key/size names.
  pattern_calls owns OTP guard signature legality for embedded expressions, including
  erlang operators, shadowing, imports and no_auto_import metadata; not guard execution.
  Shared 1M work budget now charges scalar text/metadata/edges as well as scopes/nodes;
  constants cap 10k decimal chars and 1M shift bits. Any semantic error clears all
  module binding/normalization tables. Record field/layout validation remains step17.
  92 authored OTP cases (38 legal/54 invalid), licensed unchanged/adapted helpers,
  both CLI modes/four policies, public resource limits and private 12k-level walks.
  Fresh Windows Debug 113/113 (94.20 s), full 196-unit quality pass; no suppressions.
  Logs build/patternmatch-step5; docs/pattern-semantics.md and step5 validation/evidence.
  OTP maint-29 fetched at task start (Oct1), unchanged pin 21776803ecd11f5fa948732c0ec66b8f325dedfc.
  User requested step 5 only: STOP AFTER STEP 5. Unrelated test1.toml edit preserved.

- Patternmatch step 4 complete: function-relative {clause, local} binding IDs,
  definition/read/exact-check events, tentative candidate environments and explicit
  commit. RHS-first matches; sibling definitions constrain matching but are hidden
  from sibling reads; unsafe status wins merged names. Guard assignments never bind.
  Whole argument provenance alone drives projections; extracted/body bindings stay
  unknown. Iterative walks have a 1M module budget and clear tables on exhaustion.
  Windows scope stacks own map-bearing entries indirectly to avoid allocating moves.
  26 OTP legality cases, six unchanged match_SUITE helpers, four native policies;
  fresh combined Debug 111/111 (86.79 s), all 191 production quality commands pass.
  docs/scoped-bindings.md and step4 validation/evidence; logs build/patternmatch-step4.
  Historical step-4 checkpoint: matching/guards stayed gated. Step 5 above now owns
  legality/key-size scopes; step 9 still owes clause isolation/rollback execution.
  User confirmed deleting references/otp/lib/stdlib/src/1.ir during step 4; do not
  restore it. Earlier preservation notes are historical, not current checkout state.

- Patternmatch step 3 complete (2026-10-01): atoms/booleans use runtime-owned
  spelling/word maps, UTF-8 <=255 scalars, 1..2^26 cap/default 2^20. Process-global
  non-recycled word reservations prevent foreign aliases; failed reservations may
  leave gaps. Host Term now has shared immutable atom pins; ABI/heap words stay
  one word. Descriptor version 3 / register_module_v3 appends spelling table/count;
  atom_v3(context, slot, descriptor) reads context-local slots. Old v2 call-failure
  services stay unchanged. Failed registration retains valid atom prefixes but no
  module/slots; duplicate descriptor addresses reject. Native/generated error
  payloads, arguments/results and copies validate runtime ownership. Foreign copies
  require explicit spelling remap. Factory atom/boolean validates weak lifetime.
  Fresh combined Windows Debug: 109/109, full quality 189 commands; test harnesses
  also pass tidy/Lizard. Source matching/guards remain steps 4–20. User asked step 3
  then stop. Logs: build/patternmatch-step3; docs/runtime-atoms and step3 validation.
  UTF-8 read_text is mandatory for test generators; default Windows decoding caused
  mojibake in one edit and was corrected. LLVM slot globals use getOrInsertGlobal
  for explicit module ownership; optional test payloads need actual presence guards.


## Current state and authoritative records

- `.agents/10-patternmatch.md` (2026-10-01): user requested one focused plan;
  necessary services are merged into 20 ordered steps and the separate prerequisites
  file is removed. Scope is function clauses/body matches, ordinary value patterns
  and corresponding guards. No GC, copying, workers, receive, closures, full catch/try
  or general control-flow implementation. Each step has success criteria and OTP
  tests; distinguish parsing from compiled kernels/native execution. Planning only.

- IR source comments (2026-10-01): user chose comments beside corresponding LLVM
  instructions. `--print-ir`, `--print-optimized-ir` and `--emit llvm-ir` enable
  private `annotate_source`; lowering attaches physical line-only LLVM metadata.
  CompilationState retains a scope-to-source map; textual snapshots resolve
  DebugLoc scopes directly without rereading files or walking LLVM metadata operands.
  Standard O2 retains locations/inlined-at chains; comments use owned source text,
  including macro invocation sites, includes and physical lines despite `-file`.
  User requires code-only instruction comments; filenames occur once in a header.
  Consecutive identical locations share one comment per basic block.
  SourceAnnotations preformats bounded UTF-8 comments before LLVM callbacks;
  ordinary object/bitcode requests omit the metadata. Tests extend inspection.py
  and annotated snapshot budget coverage in limits.cpp. Logs: build/source-comments/.
  Validation: all 103 CTests passed; full check-quality passed (Lizard and all
  184 production translation units through clang-tidy); changed C++ is formatted.
  Normal Debug configuration restored to BUILD_TESTING=OFF after validation.
  User-owned untracked `1.ir` was present before work and is left untouched.

- Build selection (2026-10-01): `BUILD_TESTING` defaults OFF before CTest; normal
  Debug/Windows presets set OFF. Explicit `-DBUILD_TESTING=ON` enables the 53
  helper executables/103 tests; Makefile and batch test wrappers already opt in.
  Normal build wrappers set OFF even when reusing a testing cache. Raw CMake
  preserves cache values, so use OFF to switch an existing test tree back.
  Fresh compiler/runtime Debug default build and plain runtime NMake build passed;
  zero test sources/targets and no OTP dependency with OFF. Windows Debug/Release
  runtime preset targets also exclude helpers. ON build and all 103 CTests passed.
  Full Lizard/clang-tidy gate passed over all 182 production commands; normal Debug
  preset restored OFF and its default build passed again with only the compiler executable.
  Current validation logs: `build/testing-opt-in/`; SDK explicit selection required
  `/MT`, `_ITERATOR_DEBUG_LEVEL=0` and preserving `/EHsc` for LLVM link probes.

- `.agents/01-todo.md`: feature backlog derived from the archive, with explanations,
  implementation checklists, validation obligations and optional scope. The user
  chooses the order and creates detailed plans; this does not authorize coding.

- `.agents/00-finished.md`: consolidated foundations/frontend/project/compiler/runtime
  milestone archive, historical validation and explicit unfinished-work checklist.
  Compiler steps 1–46 complete; earlier stopping points are superseded. The numbered
  plan was compacted into the archive and removed on 2026-09-30.
  Latest gate: Windows x64 Debug 103/103, zero skips, full quality (182 commands).
- Steps 24–27 commits: c86f539 (declarations/literals), f09e7b4 (parameters),
  5db13f0 (local calls), 4301401 (remote calls). Each passed fresh Windows x64
  compiler+runtime Debug, 80/80 CTests with zero skips, full Lizard/clang-tidy,
  formatting and whitespace. Final quality covered all 154 production commands.
- Current CLI owns positional/project-target AST batches and runs declaration,
  capability, binding, call, declared-type and implementation-type analysis.
  Shared backend lowers the accepted subset, optimizes at O0/O2 and emits verified
  objects in memory. Explicit --emit publishes per-module objects/IR/bitcode; project
  publication waits for all selected targets. IR/type inspection stops at its boundary.
  Generated-module registration and Windows x64 linked harness execution work.
- `--impldebug` accepts repeatable unique signed-int32 step selections/lists.
  Steps 23–27 print escaped inferred inputs/results and parameter relations on stderr;
  these are lowering inputs, not IR dumps. Frontend-only actions do not infer.
  `--verbose` prints [comp] phase/specialization events plus [pp]/[parse] source paths;
  parser-start tracing can precede include tracing because tokens stream to the parser.
- Detailed maps/contracts: `.agents/{arch,files}.md`, `docs/{compile,semantic,
  otp-reference,test-migration,project-validation}.md`, runtime design/service docs.
  Check Git state afresh; historical uncommitted-work notes are not current changes.

## Standing constraints

- C++23 and warnings-as-errors for all project targets; APIs are project-internal
  C++. User removed C compatibility after step 9: no C headers/adapters/linkage
  wrappers without a concrete new need. LLVM CallingConv::C is the native machine
  convention, not a C interoperability promise.
- No subagents unless newly authorized. Preserve user edits; check Git status before work.
- Use/build upon the term library sketches; synchronize compile-plan inventory,
  architecture and file map when changing them. Keep private layouts controlled.
- Before each implementation-step commit: fresh compiler+runtime configure/build,
  meaningful tests and `cmake --build build/debug --target check-quality`.
  Lizard CCN and clang-tidy cognitive limits are 10; no suppressions/raised thresholds.
  Format code and document function/field intent in 1–2 lines.
- Prefer real CLI/source/consumer workflows. Preserve useful ownership/budget/fault
  invariants until equivalent observable coverage exists. No product testing switches.

## OTP reference and byte contracts

- Track latest official `maint-29`, not master or a fixed release. Before future
  OTP-dependent work fetch/check upstream and follow `docs/otp-reference.md`.
  Synchronize pin, checkout, corpus hashes, grammar evidence and current docs;
  never change the reference silently in configuration/tests or rewrite old provenance.
- `references/otp-pin.cmake` currently pins
  `21776803ecd11f5fa948732c0ec66b8f325dedfc` (upstream 2026-09-22, fetched 2026-09-29).
  The upstream check at the start of steps 31-39 was unchanged. `build/debug` uses references/otp.
- Grammar and ten corpus entries matched the previous pin byte-for-byte; 344 ordinary
  productions have witnesses, 79 SSA productions are excluded. Stale-pin rejection passed.
- Existing Windows checkout has core.autocrlf=true: only manifest text/grammar files
  were normalized to LF; generate beam_opcodes.hrl with that revision's beam_makeops
  and normalize its LF bytes. New clones should use core.autocrlf=false. Do not
  globally renormalize mixed-EOL references. Check cleanliness before refresh.
- Historical migration worktree `build/test-migration/otp` uses
  `751f87b703fe5948607d08e82599ce644b772e76`; it is not the current pin.
- `.gitattributes` disables conversion under fixtures: explicitly write intended
  bytes (normally LF for new Erlang fixtures). Python on Windows defaults to legacy
  encoding/newlines; use UTF-8 explicitly and byte writes for exact LF fixtures.
- AST display: parenthesized objects, two-space indentation, name=value fields,
  iterative closing and depth-64 indentation cap. RAW/EPP are Erlang terms and were
  explicitly excluded from conversion. No AST/stage readers are implemented.

## Windows toolchain and validation

- Patternmatch step 2 (2026-10-01): call contract revision 2 keeps v1 word/symbol
  encoding but requires checked context errors. RAII host scopes preserve nested
  first failures and clear on return/unwind; generated calls share one failure exit.
  Heap/builtin services retain exact status; function_clause/badmatch own immediate
  payloads only (heap roots step 11). Startup service is register_module_v2.
  Four native fault policies cover ten failures plus ten retries each, including
  reentry and raw native exceptions after pending errors. Fresh Windows Debug
  108/108 CTests, Lizard and full clang-tidy over 185 commands pass. Stop after step 2.
  Logs/script: build/patternmatch-step2/. LLVM comparison factory avoids the SDK
  analyzer path; use typed ErrorReason parameters and no moves of trivial failures.

- Patternmatch step 1 (2026-10-01): upstream maint-29 remains 21776803; original
  untracked 1.ir preserved. New matrix/catalog and patternmatch_evidence CTest:
  81 signatures, 14 acceptance modules, 40 OTP outcomes, three real suites parsed,
  unchanged first/2 + id/1 native in four policies. O0 always bypasses specialization.
  Fresh combined Windows Debug 104/104 and full quality over 184 commands pass;
  new Python driver max CCN 4. Reusable gate/logs: build/patternmatch-step1/.
  At that checkpoint steps 2–20 remained pending; production code was unchanged.

- Use VS18 Community `VsDevCmd.bat -arch=x64 -host_arch=x64`, then LLVM/bin on PATH.
  Toolchain executables are under `C:/Program Files/LLVM`; the validated SDK is the
  pre-existing clang+llvm-23.1.2 installation under thirdparty/ (no gate-time download).
- Generic `C:/Program Files/Erlang OTP/bin/escript.exe` crashes (0xC0000005).
  Configure `ERLANG_AOT_ESCRIPT=C:/Program Files/Erlang OTP/erts-17.1/bin/escript.exe`
  (OTP 29.1.1); cache resets require the override until the installation is repaired.
- `build/compile-steps/gate.cmd N` reproduces fresh Ninja Debug, both components and
  tests ON, full build/CTest/check-quality. It sets CXXFLAGS empty and selects pinned
  clang-tidy 22.1.8 through CMAKE_PROGRAM_PATH. Scripts/logs/XML are ignored there.
  Inspect scripts before reuse; historical apply/staging scripts are not pending work.
- Native compiler/Ninja probes hung in the sandbox but worked elevated; Git writes
  also need escalation in this environment. Keep normal work in the project root.
- Windows quality defaults to two jobs after nondiagnostic analyzer failures;
  unchanged complete reruns passed. Never exclude production commands or weaken checks.
  `verify-quality.py` checks the quality database retains every original command.
- Focused checks: `lowering-focused.cmd`, `tidy-one.cmd`, step-specific logs.
  A tidy run may report system-header warning counts with no project findings;
  inspect diagnostics and exit status, not only the last command in a shell sequence.
- LLVM intrusive operand storage confused the analyzer in test inspections. Use-list
  traversal verifies data flow/call targets without suppressions; check optional
  decoded symbols. Never rebuild running tests or edit sources during quality runs.
- Historical parser_hardening stack overflow and exception/Boost analyzer failures
  were repaired before step 15: 8 MiB executable stacks, movable frontend storage,
  exact numeric conversion and separated CLI boundaries. They are not current blockers.
- Full frontend/LLVM ASan remains blocked by SDK rpmalloc duplicate CRT allocators.
  Historical Windows runtime-only ASan passed 15/15 with /MT and matching thunks;
  TestHost propagates sanitizer flags to consumers. Keep AST/raw-attribute lifetime tests.
  Additional native hosts/32-bit execution remain pending for the latest changes.

## Compiler invariants

- Expanded tokens pass directly to the parser; no print/re-lex. AST owns flat arenas,
  sources/features and checked IDs; failed forms roll back and latch failure.
  Diagnostic is aggregate data; DiagnosticError carries its mutable payload through
  std::runtime_error. LexicalError remains separate for lexer recovery.
- Project selection precedes selected-only filesystem resolution; validate every
  selected plan before processing. Errors retain manifest coordinates and target/key
  context. Sessions are never shared across files/targets. Missing suffixless project
  paths try .toml; existing paths/errors take precedence.
- Discovery splits patterns before joining a base containing [!], uses bounded
  iterative Unicode matching, identity deduplication and exact .erl extension.
  Creation uses C++23 noreplace, checks write/close and identity-based cleanup,
  refuses dangling symlinks and creates no parent directories. Windows argv is UTF-8.
- PP nuances: object macros rescan before joining the caller; include-return line
  advances only for immediate LF. Records/templates preserve syntax with legality
  deferred. Prefer eager cpp_int values over expression-template temporaries.
- Semantic symbols/type scopes use collision-free encodings. Function cycles are
  rejected iteratively; remote calls require batch membership and declared exports.
- Types remain symbolic, owner-checked and bounded. Union branches take maximum
  variable-use counts, products add. OTP permits duplicate formals (last actual wins)
  and repeated constraints; bare wildcard formals fail parsing. Nominal identities
  survive; opaque expansion is owner-only; recursive alias substitution is memoized.
  Windows map moves allocate, so traversal frames uniquely own use maps. Decimal
  bounds use validated decimal-only accumulation, not Boost's general string parser.
- Inference owns an independent graph: inputs stay top, integer singletons and
  projections propagate via freshly instantiated call summaries. Specs can warn,
  never narrow runtime representation or create guards/LLVM assumptions.
- Compilation owns ASTs, LLVM context/modules and callback results with stable
  lifetimes. Errors latch and clear all staged outputs. Verification is never cached;
  emission reverifies/clones modules and replaces buffers. LLVM 23 diagnostics take
  a DiagnosticInfo pointer; module inline asm uses Module::GlobalAsmFragment.
- Target policy: native CPU/features for native triples, generic foreign baseline,
  selected X86/ARM/AArch64 backends, PIC/Small; unsupported targets never fall back.
  Term/signature widths and alignment come from target layout, not host sizeof.
- `codegen/lowering*` consumes validated side tables, declares definitions before
  bodies, checks literals before tagging, loads original parameter slots unchanged,
  and iteratively evaluates arguments in source order. Calls forward context and
  aligned arrays (null at arity 0); remote exports become matching declarations in
  separate modules. No inbounds/unboxing/type promises or generated native linking.
- `codegen_lowering` is a real-source stage adapter, not CLI artifact/execution proof.
  Retain synthetic failure/ownership coverage until case-level replacement exists.

## Runtime implementation and reserved design

- LLVM-free Runtime owns stable contexts, CodeServer and SchedulerService. Contexts
  own lazy heap/mailbox and invalidate lifetime tokens before teardown; identities
  do not recycle. Explicit shutdown rejects live contexts; RAII drains them.
  Scheduler records are metadata only, removed before contexts/code; destroying a
  running context is busy. Registration is once-only, with rollback/retry on failure.
- Every runnable generated program must link one matching runtime through
  `ErlangAoT::generated_program`; compiler LLVM dependencies must not leak into it.
  ABI v1 uses unsigned target words, low integer tag 0xf and signed 28/60-bit payloads.
  Checked unsigned encoding/decoding avoids signed shifts/narrowing. Private heap
  prefixes respect native C++ resource alignment; tags use masks, not union aliasing.
- Raw immediate classification recognizes atom/pid/port structure, not ownership.
  Host Term admits small integers, exact empty tuple/list and owned atoms; heap/identity
  values remain rejected. Integer/empty-container copies are owner-independent and may cross
  runtimes; atom copies require same-runtime admission. Neither path copies heap graphs.
- Heap budgets are bytes in exact word multiples; accounting/requests use words.
  No backing allocation, roots, GC or graph copying yet. TermFactory weakly binds
  context lifetime and sink without roots; constructors report unimplemented.
- CodeServer freezes uniquely owned per-module registries; generic keys are exact
  name/arity/all-Term signatures. ResolvedFunction pins module/image; callable
  captures die before code image. Publication/lookup remain host-serialized.
  Typed/native templates in unverified/ and conversion utilities remain deferred.
- Builtin bridge separates Status from success-only output words, validates inputs,
  contains exceptions and propagates once-only reporting. Registered functions win;
  a bounded known-deferred catalog reports unavailable BIFs, unknown ones return
  unknown_builtin=11 silently. Reporting failure maps to diagnostic_failure.
- Send, heap/GC, worker execution and unload boundaries report without mutating
  state. Workers, reductions, queues, wakeups, receive and production BIFs are absent.
- Atom storage/bindings are implemented by patternmatch step 3; see the current
  entry above. Atom GC, worker synchronization and dense numbering are not implemented.
- Future heap roots include host, continuation, mailbox/cursor and pending transit.
  Every message, including self-send, enters a FIFO signal inbox; bounded owner
  handling copies into recipient heap before mailbox insertion. Service waiting/
  suspended processes without clearing explicit suspension. Process send acknowledges
  acceptance; scheduler replies acknowledge handling. Preserve per-sender order and
  selective-receive tail handshake. These remain contracts, not implemented behavior.
- BinaryHeapObject sketch owns an immutable shared vector<Word>, with no pool or
  owner callback; last-owner destruction releases it. Counts must exceed 64/sizeof(Word).
  Tail 0 means a full final word; nonzero counts valid high bits with zero low padding.
- Cooperative StepResult/ReductionBudget and worker policies remain sketches. The
  proposed 1:8:9 priority weighting, idle counts and realtime reservation need review.

## Historical/platform pointers

- macOS arm64 SDK 23.1.1 and earlier runtime/project sanitizer evidence remain in
  plan/docs ledgers. LeakSanitizer was unavailable. Do not present old suite sizes,
  old C ABI checks or old stopping instructions as current state.
- macOS Homebrew CXXFLAGS=-I/opt/homebrew/include can shadow SYSTEM headers; prefer
  empty CXXFLAGS on fresh configuration. Shared Boost discovery handles the matching
  linked alias and IDE fallback includes. Runtime-only needs Boost >=1.90, not
  Boost.Parser/TOML/OTP. Explicit dependency roots win.
- Windows dependency storage uses pinned Boost 1.90.0/toml++ 3.4.0 and zlib 1.3.2/
  zstd 1.5.7 fallbacks under ignored thirdparty/, with compiler/architecture/CRT
  fingerprints, locks and offline reuse. LLVM discovery must avoid MinGW .a cache
  pollution and provide Debug/Release native .lib locations.
- Batch wrappers mirror Makefile scope/options; erlangaot.bat preserves caller cwd,
  arguments/status, sends build output to stderr and refuses stale runs on failure.
- Historical logs: build/{test-migration,ast-format,windows-dependency-validation,
  dependency-validation,zlib-validation,batch-validation}/. Test migration replaced
  native suites with CLI workflows; its old 74/75 parser failure was later repaired.

- Step 28: descriptor/registration bridge and separate real-source native consumer added.
  Fresh Windows Debug 82/82 tests and full quality (156 production commands) passed.
  Native C++ service symbols use Microsoft/Itanium spellings, no C wrapper.
  Atoms remain reserved. Missing diagnostic.hpp <functional> include fixed.

- Step 29: bounded profile planner integrated privately; current source remains generic
  because there are no removable type checks. Windows 83/83 tests/full quality passed.
  LLVM 23 ConstantData has no use lists; recognize tag operands with SDK exact
  instruction comparisons, and argument data flow with use lists (no suppressions).
  Test vector.assign count/value must copy the nested source element before resizing.

- Steps 28/29 commits: 30d8260 / 2d0d7d5. Step 30 adds clone/dispatch/measured
  rollback and native guard/fallback execution; final Windows 84/84 and full
  quality (162 production commands) pass. Current source has no removable checks,
  so O2 correctly keeps it generic; source guards remain future work. Step 31 adds LLVM O2. Missing inference facts must become generic profiles, never map::at failures.
- LLVM comparison construction uses the SDK CmpInst factory; use selective replacement
  to preserve fallback calls without intrusive CallBase operand access. No suppressions.
  Final transient ShowIncludes probe access error and analyzer crash on unchanged
  preprocessor/integer.cpp passed unchanged retries. Keep all production commands.
- Read/write project text with explicit UTF-8 in Python, including markdown updates;
  Windows default decoding can silently prevent non-ASCII status replacements.

- Steps 31–39 each have their own validated commit. Standard LLVM passes/writers,
  safe artifact staging/replacement, backend CLI policies, positional/project
  orchestration, [comp] tracing, IR snapshots and type inspection are implemented.
  Artifact basename is eav1_<UTF8 hex module>__0 with target-selected suffix; project
  roots append the same encoding of target identity. Multi-file replacement is not atomic.
- LLVM callbacks must respect enabled remark filters. CMake Unicode captures need
  ENCODING UTF-8. Python test scripts must not shadow stdlib modules (e.g. types.py).
  SDK llvm-as/llvm-dis roundtrip snapshots; FileCheck is absent in the installed SDK.
- Ignored build/compile-steps/gate.cmd configures fresh Debug with both components,
  builds, runs all CTests and then full quality. Nested native consumers require VS
  x64 environment. llvm-tidy is pinned via CMAKE_PROGRAM_PATH; do not suppress checks.
- Final step 39 validation: fresh Windows x64 Debug, 93/93 CTests, zero skips,
  full Lizard/clang-tidy over 180 production units, formatting and whitespace pass.
  Historical step-39 checkpoint; steps 40–46 are covered by subsequent records.

Step 40 (2026-09-29): Public CLI objects execute in a separately configured Clang harness through the mandatory runtime link target at O0/O2; integer/immediate boundaries, projection and nested calls, ABI rejection, missing-runtime failure and explicit teardown pass. Fresh Windows x64 Debug compiler/runtime build: 95/95 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.

Step 41 (2026-09-29): 150 seeded/fixed calls agree with OTP and an independent evaluator across four optimization/specialization modes, repeated twice; annotated/unannotated pairs and incorrect specs preserve behavior. CRLF and CMake native-path issues in the new test were fixed before the passing gate. Fresh Windows x64 Debug compiler/runtime build: 96/96 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.

Step 42 (2026-09-29): Cost records cover source and synthetic guards at O0/O2 with specialization disabled/enabled. High-arity wide-union inputs remain generic, O2 outputs match byte-for-byte, 3/32/128 caps and 2x growth hold, and native guard/fallback results agree. Timings are descriptive only. Fresh Windows x64 Debug compiler/runtime build: 97/97 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.

Step 43 (2026-09-29): CLI-emitted objects pass SDK readobj/nm inspection for seven ELF, Mach-O and COFF targets at O0/O2, including architecture, exports/imports, runtime references, ABI widths/tags, exact integer endpoints and failure without publication. Foreign native execution remains pending. Fresh Windows x64 Debug compiler/runtime build: 98/98 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.

Step 44 (2026-09-29): All compiler catalog families are audited through both CLI modes at O0/O2 with verbosity on/off. Explicit executable output now fails instead of silently succeeding. Native allocation rejection preserves generated calls, heap accounting and clean teardown; atom collection is documented as a reservation without an owner. Fresh Windows x64 Debug compiler/runtime build: 99/99 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.

Step 45 (2026-09-29): Batch/AST and bounded writer byte ceilings reject without publication; injected partial/close/interrupted writes preserve destinations and clean staging. Debug STL OOM termination paths were repaired without suppressing iterator checks. Compiler-only 80/80, runtime-only Debug 16/16 and runtime ASan 16/16 pass; full compiler ASan remains blocked by the installed SDK annotation ABI. Full quality covers 182 production commands. Fresh Windows x64 Debug compiler/runtime build: 102/102 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.

Step 46 (2026-09-29): Published and executed the two-module native example at O0/O2, all artifact kinds, type/IR inspections and specialization override. Exact SDK setup, accepted semantics, ABI/runtime recipe, deferred features and 103-test inventory are documented. The example also passes focused Lizard/clang-tidy; full production quality preserves 182 commands. Fresh Windows x64 Debug compiler/runtime build: 103/103 CTests, zero skips; full Lizard/clang-tidy and whitespace checks pass. Other native hosts remain pending.
