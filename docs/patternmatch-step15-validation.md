# Pattern matching step 15 validation

Windows x64, 2026-10-02. Official maint-29 was fetched at task start; upstream,
checkout and pin remain `21776803ecd11f5fa948732c0ec66b8f325dedfc`.
Installed oracle: OTP 29.1.1 / ERTS 17.1. LLVM SDK 23.1.2, Lizard 1.24.0,
clang-tidy 22.1.8. Historical evidence retains its original versions.

| Evidence | Result |
| --- | --- |
| Fresh combined Debug compiler/runtime/testing configure and build | Pass |
| Full CTest suite | 131/131, zero skips, 286.68 s |
| Lizard and production clang-tidy units | Pass, all 244 units |
| Map OTP/native corpus | 8,010 calls in each of four policies, both CLI modes |
| Target checks | Map objects at both word widths; existing foreign object checks pass |
| Formatting and whitespace | clang-format and git diff --check pass |

Complete `map_is_size/2`, `check_map_value/3`, and `map_get_head/1` helpers are
extracted from the pinned map suite with license notices. Update, computed-key,
and nested duplicate-key case helpers have explicit ordered-clause/body-match
adaptations. Source/wrapper hashes and adaptations are retained in the evidence.
Authored cases cover exact integer/float and signed-zero keys, compound/nested
keys, independent equal maps, extra/missing keys, duplicate constraints, aliases,
key expression failure, guard construction/update, rooted error payloads,
operand evaluation order, misleading specs and local/remote retained results.

The corpus exposed missing atom registration for atoms used only in map keys;
module registration now visits embedded pattern reads. Host tests cover immutable
update rollback, canonical key ordering, ownership, expiration, retained nested
values, malformed service arrays and retry. Allocation-ordinal sweeps and
charged insertion-work exhaustion leave publication accounting unchanged.
Generated service fault tests cover guards, bodies and computed-key patterns in
all four policies, including root cleanup and successful retry.

Logs: `build/patternmatch-step15/{gate,tests,quality}.log`. No foreign native
execution, 32-bit execution or new sanitizer run is claimed. See the
[map contract](map-matching.md) and [retained evidence](patternmatch-step15-evidence.json).
A separately committed step 15a will freeze OTP-generated native inputs and
expected values as project-owned test data, per the user's additional request.
Bitstrings, records, remaining guards, optimization and finalization remain in
steps 16–20. GC, cross-process copying and scheduling remain outside this plan.
