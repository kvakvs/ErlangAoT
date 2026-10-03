# Pattern matching step 20 validation

Completed 2026-10-03 on Windows x64. All steps 1–20 and added step 15a of
[the pattern/guard plan](../.agents/11-plan.md#completed-patternmatch) are complete within its
function-clause and body-match scope. [Final evidence](patternmatch-step20-evidence.json)
records concrete corpus identities, all 81 guard rows, individual test results and log hashes.

## Reference and fresh gate

Official maint-29 was fetched at task start; upstream, pin and clean checkout
remain `21776803ecd11f5fa948732c0ec66b8f325dedfc`. Source/grammar/corpus identities
are synchronized. Oracle: OTP 29.1.1 / ERTS 17.1. Host/SDK LLVM 23.1.2; Visual
Studio 18 Community x64 and Windows SDK 10.0.26100.0; C++23, /MT and iterator
level 0; Boost 1.90.0, toml++ 3.4.0; Lizard 1.24.0, clang-tidy 22.1.8.

Fresh combined Debug configuration, compiler/runtime/testing ON, OTP audits OFF
and deliberately absent OTP paths: build passed; **124/124 CTests**, zero skips,
167.43 s. Lizard passed unchanged **CCN 10**; clang-tidy passed all **258
production units** with unchanged cognitive complexity 10 and full compiler flags.
Changed C++ is clang-formatted; whitespace checks pass. Logs are retained under
`build/patternmatch-step20/{gate,tests,quality}.log`.

## Corpus and workflows

Explicit `regenerate.py --corpus all --check` reproduced all **19 corpora**;
`all-regeneration.log` retains the results. Routine tests verify committed hashes
and consume owned goldens, requiring neither an OTP installation nor checkout.
Original licenses and complete-helper/selected-clause/adaptation distinctions remain
in the manifests; historic generator hashes describe their original regeneration.
No Common Test suite execution is claimed.

There are **67,634 native expected values/error reasons**, plus **106 semantic
acceptance rows** counted separately. Every native corpus runs both positional
and project drivers at O0/O2 with specialization enabled/disabled, local and remote
calls, and two consumer executions per combination: **1,082,144 golden comparisons**.
Consumers link the separately configured real runtime without LLVM. Separate artifact
directories per driver keep publication independent while sharing per-policy consumer
builds. Negative missing-runtime links remain mandatory.

The closure corpus adds **1,969 outcomes**, deterministic seed `0x29A07`, nested
depth **64**, tuple/list/map width **255**, 2,048-bit values and **128 ordered
alternatives**. Authored bounded extensions exercise repeated binding, exact/mixed
comparison, rooted nested construction/extraction, candidate rejection and remote
recovery. The test reconciles all fixture inventories/hashes and requires concrete
function/resolver/lowering/runtime mappings for every admitted catalog signature.

The first full run passed 123 tests but exposed the test transport's depth-256
serialization cap when wrapping a 255-cell list in a returned tuple. Its bounded
test-only allowance is now 512. No compiler/runtime/quality threshold changed.
The final fresh run above passes; initial logs remain `initial-{gate,tests}.log`.

## Completed contract and failure evidence

[The semantic matrix](patternmatch-matrix.md) maps every in-scope obligation to
native or diagnostic coverage. Clause-local candidate bindings publish only on
success; body matches evaluate the RHS once, enforce exact rebinding and return
it. Map keys read incoming bindings; binary sizes may read earlier segments;
aliases do not acquire sibling key/size definitions. Owned atoms, arbitrary
integers, finite floats, tuples, proper/improper lists, strings, maps, bitstrings
and ordinary tuple records share checked rooted construction/access/comparison.

Grouped/strict/lazy guards preserve source order. Reached semantic errors reject
their guard alternative; infrastructure/resource/ownership failures stop execution.
Bodies propagate typed `error` reasons and owned offending values. Runtime admission
checks ownership before access; result/error handoff precedes root cleanup.
[Binding facts](binding-facts.md) remain separate from specs: extraction/unproved
joins stay conservative, and service-success/shape proofs dominate dependent loads.
Existing specialization/work/IR limits retain verified generic fallback.

The full gate includes allocation/root/service fault policies, retained nested
results/errors across later allocation, first-error preservation, same-context retry,
ABI/width rejection and teardown. Real multi-representation budget fixtures test
inference fallback, module/batch bytes, IR snapshots and whole-draft rollback.
Publication/write/resource failures erase staged artifacts and allow later recovery.
Fault seams and private inaccessible ceilings remain labeled separately from source
execution. The [failure ABI](generated-call-failures.md) retains descriptor revision
4 and checked channel revision 2; this finalization requires no new ABI revision.

The [documented example](compile.md#run-the-compiled-module-example) runs in all
four policies and prints `42`, `-7`, `record`, `map`, `binary`, `list`, `integer`,
`other`, one per line. Its remote demo classifies constructed values using ordered
record/map/bitstring/list patterns and the inclusive integer-range guard.

## Remaining owners and platform evidence

All **81 catalog rows** are reconciled: **77 implemented on admitted values**,
four explicitly gated (`self/0`, `node/0,1`, native `is_record/1`). Function/pid/
port/reference predicates only have negative classifications on admitted values;
their positive representations require F07/F18. Qualified, unqualified and skipped
calls retain legality/availability diagnostics. Installed-oracle native-record loader
and legacy-import compiler limitations remain recorded in steps 17/18.

Additional case/if/maybe/comprehension, catch/try, closure, receive and dynamic-call
contexts remain F16/F18/F19/F20/F25. Record updates/record_info/native forms remain
F17. Recursion, GC, graph copying, scheduling, messaging, generic production builtin
registration and executable startup/linking retain their owners. Backlog updates
close only delivered source/representation/service slices.

Available native evidence is **Windows x64**. Linux, Apple Silicon and native
32-bit runners and a new compiler/frontend sanitizer run remain unavailable.
Foreign-object inspection, 32/64-bit IR/layout verification and original-suite
parsing are separate evidence. Historical macOS/runtime-ASan validation keeps its
original dates, revisions and instrumentation scope. V01/V02 remain open.
