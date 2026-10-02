# Pattern matching step 13 validation

Windows x64, 2026-10-02. The maint-29 pin remains
`21776803ecd11f5fa948732c0ec66b8f325dedfc`, checked at task start. Installed oracle:
OTP 29.1.1 / ERTS 17.1. LLVM/Clang 23.1.2, Lizard 1.24.0, clang-tidy 22.1.8.

| Evidence | Result |
| --- | --- |
| Fresh combined Debug compiler/runtime/testing configure and build | Pass |
| Full CTest suite | 127/127, zero skips, 145.19 s |
| Lizard and all production clang-tidy units | Pass, all 231 units |
| OTP/native integer corpus | 16,065 calls in all four policies, both CLI modes |
| Foreign object inspection | Seven 32/64-bit ELF/Mach-O/COFF targets; no foreign execution |
| Formatting and whitespace | clang-format and git diff --check pass |

The corpus retains complete unchanged trycatch_SUITE:my_div/2, my_add/2 and
beam_bounds_SUITE:bnot_bounds_2_coverage/1, with both upstream license notices.
Arithmetic/bitwise/shift wrappers explicitly adapt the bounds-suite operations
without its higher-order/private BEAM harness. Source paths, hashes, complete
helper bodies, declarations, wrapper hashes and adaptation notes are retained in
the evidence JSON. Authored cases cover both signed payload boundaries, int64
endpoints, large signed literals, promotion/demotion, independent allocations,
nested repeated patterns, guard alternatives, badarith versus abs/1 badarg,
first-failure ordering, local/remote calls and misleading specs. Seed 29013 adds
256 signed operand pairs of up to 400 bits for carry/borrow and mixed-width work.

All four O0/O2 specialization on/off policies use the separately linked LLVM-free
consumer and both positional/project publication workflows. IR checks require
double-width add/subtract/multiply, checked fallback and rooted integer literals;
both selected target widths emit integer patterns and operators. Seven-target
inspection checks native integer-service imports. Earlier overflow-rejection tests
now require promotion. Source literals over the decimal ceiling reject with located
diagnostics before publishing artifacts, for both expressions and patterns.

Host invariants check canonical zero, signed/leading-zero decimal syntax, full
int64 round trips, checked narrowing, independently allocated equality, demotion,
stable growth, foreign rejection and expiration. Allocation-ordinal sweeps cover
decimal construction and the exact arithmetic fallback, including temporary limbs,
backing and object publication. Failure leaves the output untouched and restores
accounting; later calls succeed and teardown balances tracked allocations. MSVC initializes two process-wide stream locale facets on first formatting; the isolated leak counter warms those standard-library caches before per-integer accounting, then verifies every allocation ordinal.
Generated service faults run in all four policies and retain exact infrastructure
statuses across numeric guards/body calls, with rooted outputs and clean recovery.
A real excessive shift bypasses both semicolon alternatives and later clauses.

The first oracle comparison caught abs/1's badarg distinction and it was corrected.
Quality review split capability traversal, separated owned calculation from checked
Term transport, and replaced opaque addition/decimal accumulation with explicit
bounded word carry/borrow. LLVM computes fast-path results at twice target width,
proves payload bounds before encoding, and forms joins through SSAUpdater. No
quality thresholds or suppression settings changed. An obsolete quality run was
stopped so newly added units could be freshly configured/built before final checks.

Local logs are in `build/patternmatch-step13/`. Other native runners and new
sanitizer execution were unavailable/not attempted. Resource ceilings are explicit
infrastructure outcomes, not claims about OTP's system_limit threshold. Floating
point is step 14; collection, graph copying and scheduling remain outside this plan.
See [integer contract](integer-matching.md) and [generated roots](generated-roots.md).
