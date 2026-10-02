# Pattern/guard step 7 validation — 2026-10-02

Implements **Resolve guard calls and implement immediate services (F12/F26 slice)**
from [the plan](../.agents/10-patternmatch.md); see [the service contract](immediate-guards.md).
Official maint-29 was fetched at the start of steps 6–8. Upstream, clean checkout
and pin remain `21776803ecd11f5fa948732c0ec66b8f325dedfc`. Grammar/corpus hashes
are unchanged; the evidence manifest additionally pins `beam_type_SUITE.erl`.
Installed oracle: OTP 29.1.1 / ERTS 17.1. Native runner: Windows x64, Visual Studio
18 environment, Clang/LLVM 23.1.2. Quality: Lizard 1.24.0, clang-tidy 22.1.8.
Historical records retain their original revisions/outcomes.

`patternmatch_services` compares 1,689 source calls with OTP through unmodified CLI
objects and the separate runtime-only consumer at O0/O2 with specialization on/off,
using positional and project modes. Fourteen predicates, eight comparisons, min/max,
queries, canonical booleans, Unicode atom order, integer endpoints, cross-type
inputs, misleading specs, nested body errors and retry agree on values/reasons.
Every query's wrong-type outcomes distinguish guard rejection from body `badarg`.

Licensed adaptations retain the first `guard_SUITE:bool/1` guarded clause, without
its step-9 fallback. Authored scalar kernels adapt the integer/non-number checks in
`beam_type_SUITE:numbers/1`; float construction/arithmetic and Common Test remain
outside the slice. Resolution fixtures adapt suppression/local/import declarations
from `overridden_bif_SUITE`. Source assertions and hashes prevent silent drift.
[Evidence](patternmatch-step7-evidence.json) records source/wrapper/fixture hashes,
exports, adaptations and the 31 OTP legality cases.

Resolution tests cover qualified/unqualified calls, imports, local shadowing,
all/selective suppression, legacy predicates, wrong arities, dynamic/user calls,
unreachable illegal assignments/calls and legal unavailable signatures. Rejected
cases run both CLI modes and four policies, preserve prior artifacts, and recover
on later valid compilation. Type and before/after IR inspection cover all kernels;
LLVM verification remains mandatory.

Four source-generated fault workflows inject exact out-of-memory, resource-limit,
wrong-owner, unavailable-service and internal statuses into guard/body/nested calls.
They prove propagation, no output use, cleanup and successful retry. A head mismatch
enters no guard service. A generic registered builtin cannot replace the authorized
guard. Private boundary checks reject malformed/foreign words and leave semantic
badarg channel-free. A tiny-budget invariant proves transactional resolution/retry.
These labeled fault objects rename only the service declaration; native differential
execution uses ordinary unmodified CLI objects.

Integration testing repaired duplicate traversal of negative integer literals and
specialization's generated-callee lookup for builtins. Existing identity/direct-call,
artifact, provenance, bounded frontend and seven-target object regressions remain.
No quality thresholds, suppressions or excluded commands were introduced.

Final fresh combined Debug configure/build, **119/119 CTests**, zero skips, full
Lizard and clang-tidy over **205 production units**, formatting and whitespace checks
pass. Local gate/logs: ignored `build/patternmatch-step7/`.

Linux x86/ARM, Apple Silicon and native Windows 32-bit runners remain unavailable.
Foreign objects are inspection evidence only; no new sanitizer result is claimed.
Grouping/boolean operators remain step 8, ordered dispatch step 9, and later
representations and guard services remain with their planned owners.
