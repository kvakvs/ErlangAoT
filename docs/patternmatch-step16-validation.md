# Pattern matching step 16 validation

Windows x64, 2026-10-02. Official maint-29 was fetched at task start; upstream,
clean checkout and pin remain `21776803ecd11f5fa948732c0ec66b8f325dedfc`.
Installed oracle: OTP 29.1.1 / ERTS 17.1. LLVM SDK 23.1.2, Lizard 1.24.0,
clang-tidy 22.1.8. Historical records retain their original evidence.

| Evidence | Result |
| --- | --- |
| Fresh combined Debug compiler/runtime/testing configure and build | Pass; OTP audits OFF and deliberately absent OTP paths |
| Full CTest suite | 120/120, zero skips, 113.81 s |
| Lizard | Pass; CCN limit 10 unchanged |
| clang-tidy | Pass; all 253 production units |
| Project-owned bitstring corpus | 8,826 calls in each of four O0/O2/specialization policies, both CLI modes |
| Formatting / whitespace | clang-format dry run and git diff --check pass |
| Explicit OTP regeneration check | Retained inputs/results reproduced byte for byte |
| Foreign object inspection | Two modules each at i686 Windows, x64 Windows and AArch64 Linux; headers, architecture, address width and service symbols checked |

The five original `bs_*` suites listed in the plan parse with real includes and
feature flags. This is syntax evidence only. Complete `bin_tail_c/2`,
`bin_tail_c_dead/2`, `bin_tail_c_var/2`, `bin_tail_d_dead/2` and `bin_tail_d_var/2`
helpers retain their unchanged bodies and license notices. String, invalid-size,
zero-width, UTF and retained-tail adaptations are identified in the manifest;
recursive traversal is deferred to F21. Hashes distinguish upstream source,
adapted modules, oracle inputs and expected values.

Native coverage includes partial bytes, signed arbitrary fields, explicit/native
endianness, unit boundaries, UTF errors, default/explicit sizes, truncation,
dependent sizes, repeated variables, aliases, retained remote tails, binary parts,
guard construction/alternatives, exact map keys and misleading specs. Additional
OTP boundary checks established zero-width float extraction, float16 rounding,
nonfinite encoded construction, literal float coercion without pattern rounding,
and rejection of explicit `:all`. Golden results exposed and corrected the two
latter implementation differences before the passing suite.

Focused host tests cover inline padding, shared nonaligned views, foreign/interior
and expired words, heap budget charging, malformed arrays, unchanged failure
outputs and retry. Allocation-ordinal sweeps verify rollback and resource release
for small/shared construction and extraction. Four generated service workflows
verify roots for borrowed inputs and both outputs, exact infrastructure statuses,
guard/body/pattern cleanup and successful later recovery. Heap cells remain
allocated with their heap backing until teardown and release of its final host
pin; GC and cross-process graph copying are deferred.

Eleven added OTP-backed semantic cases verify construction modifier rejection,
including skipped guard operands, the admitted unit-256 boundary in construction
and matching, and invalid explicit UTF sizes. Both CLI modes preserve existing
artifacts after these errors and recover for later valid compilations.

Logs are retained under `build/patternmatch-step16/`. Foreign inspection does not
prove native execution. Linux, Apple Silicon and 32-bit native runners remain
unavailable, and no new sanitizer run is claimed. See the
[bitstring contract](bitstring-matching.md) and
[retained evidence](patternmatch-step16-evidence.json). Steps 17–20 remain open.
