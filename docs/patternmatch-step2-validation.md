# Pattern/guard step 2 validation — 2026-10-01

Implements **generated-call failure propagation (F20/F02 slice)** from
[the plan](../.agents/10-patternmatch.md). The [contract](generated-call-failures.md)
uses a checked context channel and descriptor/runtime revision 2. The executable
source subset is unchanged. Steps 3–20 remain pending.

| Dependency / provenance | Identity |
| --- | --- |
| Official reference | `maint-29`, fetched again on 2026-10-01; unchanged |
| Checkout / pin | `21776803ecd11f5fa948732c0ec66b8f325dedfc` |
| Installed oracle | OTP `29.1.1`, ERTS `17.1`, separate from the source checkout |
| Native runner | Windows x86_64, Visual Studio 18 x64 environment |
| Compiler / SDK | Clang/LLVM `23.1.2` |
| Quality tools | clang-tidy `22.1.8`, Lizard `1.24.0` |

The checkout preserves its original untracked `lib/stdlib/src/1.ir`. Pin, grammar,
corpus hashes and step-1 source identities did not change. Historical validation
records retain their original revisions/results.

| Check | Result |
| --- | --- |
| Fresh Debug configure and combined build | Pass, compiler/runtime/testing enabled |
| Full CTest after final changes | **108/108 passed**, zero failures/skips, 94.05 seconds |
| Four new native fault workflows | Pass: O0/O2 with specialization policy on/off |
| Registration / wrong ABI | Revision-1 descriptors reject before metadata; mismatched widths reject; missing-runtime links fail on revision-2 startup service |
| Existing CLI/OTP workflows | Pass: unchanged first/2 and id/1, original suite parsing, stale hash rejection, differential calls, incorrect specs, positional/project modes and failed-batch nonpublication |
| Cross-target object/layout checks | Pass for all seven existing targets, including 32-bit target words |
| Production quality | Full Lizard/clang-tidy pass over **185** production translation units; unchanged checks/thresholds and original compiler flags |
| New C++ fault harness complexity | Separate Lizard passes at threshold 10 |
| Formatting / whitespace | clang-format and git diff --check pass |

The new authored `failure_answer.erl` and `failure_client.erl` exercise real parser,
semantic analysis and generic local/remote call lowering. The emitter replaces
only the leaf, subsequent-argument and consuming-call bodies with labeled native
fault seams before optimization. This is explicit transport/fault evidence;
source `function_clause` and `badmatch` generation remains steps 6 and 10.

Each of the four modes executes ten failures and ten successful retries through
the same resolved entry/context. Cases include the existing heap allocation
service rejection, diagnostic delivery failure, silent function_clause/badmatch,
rejected heap payloads, throwing native builtins, structured builtin errors,
reentrant generated calls, and raw native-entry exceptions with/without a pending
Erlang error. The harness checks skipped later arguments/calls, first-failure
preservation, success-only builtin outputs, zero heap accounting, channel cleanup,
payload survival after retry, and explicit context/runtime teardown. Immediate
payloads own their words; this does not establish heap lifetime/root correctness.

One full run initially exposed excess textual IR growth in the existing bounded
measurement test. Shared per-function failure exits removed repeated return
blocks; the original size limit and every quality threshold remain unchanged.
No unavailable feature was removed from a fixture to make these tests pass.
O0 bypasses specialization internally, and these source calls remain generic at
O2 because they have no removable type checks; the four runs validate policy
compatibility, not new pattern-specialized clones.

Linux x86/ARM, Apple Silicon and native Windows 32-bit execution remain unavailable.
Foreign object inspection is not native execution. No new sanitizer result is
claimed. Heap payload roots, matching, guards, catch/try, stacks, throw/exit and
suspension remain outside step 2.

Reproduce in the documented native toolchain environment:

```text
cmake --preset debug --fresh -DBUILD_TESTING=ON -DERLANG_AOT_BUILD_COMPILER=ON -DERLANG_AOT_BUILD_RUNTIME=ON <local dependency/toolchain overrides>
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure -j 4
cmake --build build/debug --target check-quality
```

Local gate scripts and detailed logs are retained under ignored
`build/patternmatch-step2/`; all source fixtures and test registration are tracked.
