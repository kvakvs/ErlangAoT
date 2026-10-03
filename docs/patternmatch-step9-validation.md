# Pattern/guard step 9 validation — 2026-10-02

Implements **Integrate ordered function clauses (F15 slice)** from
[the plan](../.agents/11-plan.md#completed-patternmatch); see [the contract](ordered-clauses.md).
Official maint-29 was fetched at task start; upstream, clean checkout and pin
remain `21776803ecd11f5fa948732c0ec66b8f325dedfc`. Grammar/corpus and source hashes
remain unchanged. Installed oracle: OTP 29.1.1 / ERTS 17.1. Native runner:
Windows x64, Visual Studio 18 environment, Clang/LLVM 23.1.2. Quality tools:
Lizard 1.24.0, clang-tidy 22.1.8. Historical evidence is retained.

`patternmatch_clauses` compares **1,020 calls** with OTP using public CLI objects
and the separate runtime-only native consumer. O0/O2 with specialization on/off
cover positional and project modes; each native stream runs twice and checks
failure-channel cleanup after every invocation and explicit runtime teardown.

Complete unchanged `guard_SUITE:bool/1` and `csemi4_orelse_{a,b,c,d}/4` helpers
include their fallback clauses, closing step 8's deferred execution checks.
`match_SUITE:char_alias_1/1` retains its integer aliases and fallback; the float
clause is explicitly omitted until step 14. License notices are retained.
[Evidence](patternmatch-step9-evidence.json) records source hashes, retained
helpers, adaptations, declarations and generated wrapper hashes.

Authored kernels cover overlapping heads, repeated-variable mismatch, aliases,
guard rejection and semantic errors, same-name clause isolation, exhausted
selection through local/remote callers, later-clause calls, selected-body errors
that must not try fallback, incorrect specs and successful retry. A 129-clause
kernel exercises wide dispatch. Type and both IR modes succeed. Result joins
retain a common argument projection and drop conflicting projections; distinct
constant results form a conservative union. This closes step 4's clause-isolation
and failed-candidate rollback execution obligations.

Seven negative batches check unsupported later/unused bodies and guards,
undefined/unknown local/remote targets, later-clause recursion and leaked names.
Every optimization policy and both CLI modes reject with locations, preserve
existing output, publish no partial batch and permit later successful compilation.
The existing four injected-service workflows now include a function with an
unconditional fallback clause: exact allocation/resource/ownership/unavailable/
internal statuses terminate selection and successful retry still works.

The initial run found a return-source annotation regression and an invalid test
combination of --print-types with -O2. The return now retains the original final
expression location, and the test uses the documented inspection policy. Both
failures passed the fresh rerun; no expected behavior was weakened.

Fresh combined Debug configure/build: **121/121 CTests**, zero skips (102.81 s).
Full Lizard and clang-tidy pass over **208 production units**; formatting and whitespace checks pass. The inference result join was separated into a helper to satisfy the existing cognitive-complexity limit; no thresholds or suppressions changed.
Local logs: ignored `build/patternmatch-step9/`.

Linux x86/ARM, Apple Silicon and native Windows 32-bit runners remain unavailable.
The full gate retains existing foreign-object/32-bit layout evidence separately
from native execution; no new sanitizer run is claimed. Steps 10–20 remain open.
