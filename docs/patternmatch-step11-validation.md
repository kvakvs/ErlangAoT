# Pattern matching step 11 validation

Windows x64, 2026-10-02. Step 11 implements stable bounded backing and generated
root scopes. Compound admission and retained compound result/error tests remain
explicit step-12 requirements. No collection, graph copying or suspension is claimed.

The official maint-29 branch was fetched at the start of this continuation and
remained `21776803ecd11f5fa948732c0ec66b8f325dedfc`; checkout and pin were clean and
unchanged. Existing provenance-checked OTP kernels were rerun, with no new helper
adaptations in this infrastructure step. The oracle is installed OTP 29.1.1 / ERTS
17.1, independently recorded from source evidence. Compiler SDK: LLVM/Clang 23.1.2;
quality: Lizard 1.24.0 and clang-tidy 22.1.8.

| Evidence | Result |
| --- | --- |
| Fresh combined Debug configure, compiler/runtime/testing enabled | Pass |
| Build and all CTests | 123/123, zero skips, 150.81 s |
| Full production Lizard / clang-tidy | Pass, all 213 production units |
| Formatting and whitespace | clang-format dry run and git diff --check pass |
| Native platform | Windows x64 only |
| Foreign object inspection | Seven 32/64-bit ELF/Mach-O/COFF targets, O0/O2; revision-4 descriptors and root enter/leave symbols |

`runtime_memory` checks zero/overflow/alignment rejection, maximum lazy budgets,
exact capacity exhaustion, preserved addresses across growth, nested-reservation
rejection, move/rollback accounting, resource destruction and expired access.
`runtime_lifecycle_failure` sweeps backing/chunk/resource-index and root-buffer/index
allocation failures; unpublished objects/roots roll back and teardown balances
allocations. Earlier context/module/atom failure sweeps still pass.

`runtime_roots` checks zero initialization, nested accounting, result transfer before
pop, preservation of an enclosing caller, restoration after incomplete nested exits,
word/frame bounds, LIFO rejection, invalid contexts and recovery.
`codegen_service_{O0,O2}-{on,off}` observes roots during reached predicates and forces
production root-entry budget failures at outer/nested depths, proving no body work,
exact infrastructure status, no leaked frames and successful retry. Existing generated
fault workflows now require zero roots after service errors and native exceptions;
GC remains the diagnostic-failure seam after allocation becomes implemented.

All earlier source/OTP clause, boolean, body-match, call and retry suites pass under
the new scopes. Positional/project workflows, failed publication, ABI rejection and
both IR inspections pass. Root-entry failure introduces an additional return path;
IR tests now inspect preserved constants/argument projections and per-block source
annotations rather than assuming a single entry-block return. LLVM may retain a
helper after root bookkeeping raises its inlining cost; both retained calls and
inlined provenance remain checked.

Registration rejects descriptor revisions 1, 2 and 3. Native service manglings were
checked against Clang declarations for x86/x64 MSVC and x64 Itanium; foreign objects
also inspect both target word widths. No foreign runtime execution or new sanitizer
run occurred. Historical validation records retain their original revisions.

Logs are local under `build/patternmatch-step11/`: `gate.log`, `tests.log`,
`quality.log`, focused analysis and intermediate regression logs. Contracts:
[generated roots](generated-roots.md), [process memory](runtime-memory.md).

The initial full clang-tidy run exited without a diagnostic for unchanged
`compiler/src/ast/control.cpp`. Its isolated invocation passed; the complete
`check-quality` rerun passed all 213 units with unchanged settings. The initial
log is retained as `quality-initial.log`; no finding was suppressed or bypassed.
