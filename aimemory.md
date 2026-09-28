# Working memory — 2026-09-28

## Current state and authoritative records

- `.agents/00-finished.md`: completed foundations/frontend/project work and migration;
  project steps 1–22 complete. `.agents/04-compile.md`: steps
  1-30 complete; user requested stopping after step 30. Do not start step 31 implicitly.
  The compile plan's compact validation ledger retains per-step historical evidence.
- Steps 24–27 commits: c86f539 (declarations/literals), f09e7b4 (parameters),
  5db13f0 (local calls), 4301401 (remote calls). Each passed fresh Windows x64
  compiler+runtime Debug, 80/80 CTests with zero skips, full Lizard/clang-tidy,
  formatting and whitespace. Final quality covered all 154 production commands.
- Current CLI owns positional/project-target AST batches and runs declaration,
  capability, binding, call, declared-type and implementation-type analysis.
  Private backend lowers the accepted subset to verified LLVM modules/objects;
  Generated-module registration and linked harness execution work; CLI artifact
  publication remains later.
- `--impldebug` accepts repeatable unique signed-int32 step selections/lists.
  Steps 23–27 print escaped inferred inputs/results and parameter relations on stderr;
  these are lowering inputs, not IR dumps. Frontend-only actions do not infer.
  `--verbose` separately prints [pp] source/resolved includes and [parse] source paths;
  parser-start tracing can precede include tracing because tokens stream to the parser.
- Detailed maps/contracts: `.agents/{arch,files}.md`, `docs/{compile,semantic,
  otp-reference,test-migration,project-validation}.md`, runtime design/service docs.
  Check Git state afresh; historical uncommitted-work notes are not current changes.

## Standing constraints

- C++23 and warnings-as-errors for all project targets; APIs are project-internal
  C++. User removed C compatibility after step 9: no C headers/adapters/linkage
  wrappers without a concrete new need. LLVM CallingConv::C is the native machine
  convention, not a C interoperability promise.
- No subagents unless newly authorized. Preserve user edits, particularly the
  Makefile clean target and runtime sketches; do not touch/stage those owned edits.
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
  `21776803ecd11f5fa948732c0ec66b8f325dedfc` (upstream 2026-09-22, fetched 2026-09-28).
  The upstream check at the start of steps 28-30 was unchanged. `build/debug` uses references/otp.
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
  Host Term admits only small integers and exact empty tuple/list; heap/identity
  values remain rejected. Immediate copies/add are owner-independent and may cross
  runtimes; these trivial operations must not be reused for future heap graphs.
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
- Reserved atom design (initialization deferred beyond step 28): one runtime storage; stable non-recycled dense IDs,
  default cap 2^20 / hard 2^26. Compiler emits spellings/slots, never IDs; initialize
  read-only bindings through runtime calls before publication and retain module roots.
  No per-use interning. Preserve name_atom(), ExportName arity and rooted metadata.
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
  so O2 correctly keeps it generic; source guards and the LLVM O2 pipeline remain
  future work. Missing inference facts must become generic profiles, never map::at failures.
- LLVM comparison construction uses the SDK CmpInst factory; use selective replacement
  to preserve fallback calls without intrusive CallBase operand access. No suppressions.
  Final transient ShowIncludes probe access error and analyzer crash on unchanged
  preprocessor/integer.cpp passed unchanged retries. Keep all production commands.
- Read/write project text with explicit UTF-8 in Python, including markdown updates;
  Windows default decoding can silently prevent non-ASCII status replacements.
