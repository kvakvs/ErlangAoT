# Pattern matching step 14 validation

Windows x64, 2026-10-02. Official maint-29 was fetched at task start; upstream,
clean checkout and pin remain `21776803ecd11f5fa948732c0ec66b8f325dedfc`.
Installed oracle: OTP 29.1.1 / ERTS 17.1. LLVM SDK 23.1.2, Lizard 1.24.0,
clang-tidy 22.1.8. Historical records retain their original versions.

| Evidence | Result |
| --- | --- |
| Fresh combined Debug compiler/runtime/testing configure and build | Pass |
| Full CTest suite | 129/129, zero skips, 131.48 s |
| Lizard and all production clang-tidy units | Pass, all 238 units |
| Float OTP/native corpus | 14,436 calls in each of four policies, both CLI modes |
| Target checks | Float objects at both word widths; existing seven-target object inspection passes |
| Formatting and whitespace | clang-format and git diff --check pass |

The corpus retains `float_SUITE:pc/3` unchanged with its license notice and labels
the ordered-clause adaptation of `beam_type_SUITE:float_compare/1`. Pinned source
paths/hashes, helper details and wrapper hashes are in the evidence JSON.
Authored cases cover finite endpoints/subnormals, signed zero, ties, large
integer neighbors, independent equality, nested exact/numeric equality, literal
and repeated patterns, conversions, wrong operands, overflow, guard rejection,
misleading specifications, local/remote calls and retained values. Seed 29014
adds 64 finite floats and their neighboring arbitrary integers.

All four O0/O2 specialization on/off policies execute through the separately
linked runtime consumer with exact binary64 bit transport. The oracle exposed
unary plus's conversion of negative zero to positive zero; the implementation
now agrees. Existing integer corpus and generated service fault policies pass.
Host tests cover nonfinite rejection before allocation, immutable independent
values, foreign admission, retained values across growth, expiration, malformed
literal bytes and retry. Allocation-ordinal sweeps verify publication/accounting
rollback and balanced teardown. Generated numeric failures preserve infrastructure
status and clear roots without selecting another guard alternative.

LLVM float literals call the checked byte service and contain no floating fast-math
flags. Runtime conversion rounds integer magnitudes once (ties to even) and mixed
ordering compares exact integer values without lossy casts. Existing operation
IDs are unchanged. Quality findings were resolved by small comparison helpers;
no thresholds or suppressions changed. An obsolete preliminary test run was
stopped after an operation-ID edit caused mismatched compiler/consumer builds;
only the final fresh gate above is completion evidence.

Logs: `build/patternmatch-step14/{gate,tests,quality}.log`. Other native runners,
32-bit execution and new sanitizer runs were unavailable/not attempted. See the
[float contract](float-matching.md) and [retained evidence](patternmatch-step14-evidence.json).
Maps, bitstrings, records and the remaining guard/optimization/finalization work
continue in steps 15–20; GC, copying and scheduling remain outside this plan.
