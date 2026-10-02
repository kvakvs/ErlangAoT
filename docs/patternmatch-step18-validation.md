# Pattern matching step 18 validation

Completed 2026-10-03 on Windows x64. Official maint-29 upstream, clean checkout
and pin remain `21776803ecd11f5fa948732c0ec66b8f325dedfc`, fetched at step start.
Oracle OTP 29.1.1 / ERTS 17.1; LLVM SDK 23.1.2, Lizard 1.24.0 and clang-tidy 22.1.8.

Fresh combined Debug compiler/runtime/testing ON, OTP audits OFF and deliberately
absent OTP paths: build passed, **122/122 CTests**, zero skips, 117.71 s.
Lizard passed unchanged CCN 10; clang-tidy passed all **257 production units**.
Formatting and whitespace checks pass. Logs: build/patternmatch-step18/
{gate,tests,quality}.log. Historical evidence remains unchanged.

The [81-row catalog audit](patternmatch-step18-evidence.json) identifies each
signature's resolver, lowering, runtime owner and named native helpers. The owned
corpus executes 5,033 OTP outcomes in four policies, both CLI modes and local/remote
calls. It retains complete range/min-max/map helpers and labeled conversion
adaptations. Explicit regeneration --check reproduces its inputs and results.
The services and patterns resolution manifests now identify range-BIF availability.
An old negative-test expectation was corrected to use the still-unavailable self/0;
the initial failed run is retained in initial-tests.log. The dispatcher cognitive
complexity finding was fixed by flattening its branch; initial-quality.log retains
that diagnostic. All 17 current corpora reproduce with explicit OTP regeneration.

Twenty-two semantic cases exercise legacy record shadowing, old-name suppression,
modern suppression, illegal operator/arity calls and each unavailable signature
qualified/unqualified and reached/skipped. All negative batches preserve the output
sentinel under every policy and both drivers. Four native service-fault policies
include range guards/bodies, preserving infrastructure status, cleanup and retry.
Existing allocation sweeps, ABI rejection and staged container tests pass.

See [guard services](guard-services.md) for bounds-first is_integer/3 behavior,
guard construction/map updates, alias rules and the four dependency-blocked
signatures. Function/pid/port/reference predicates classify admitted values only;
no positive native representation claim is made. The installed OTP legacy-import
SSA crash remains an oracle limitation, with explicit project authorization rejection.
Native Linux, Apple Silicon and 32-bit runners and a new sanitizer run are unavailable.
