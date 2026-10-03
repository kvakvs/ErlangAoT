# Pattern/guard step 4 validation — 2026-10-01

Implements **Introduce scoped bindings and conservative value facts** from
[the plan](../.agents/11-plan.md#completed-patternmatch). The [binding contract](scoped-bindings.md)
describes identities, tentative publication, body scopes, budgets and conservative
facts. Steps 5–20 remain pending.

| Dependency / provenance | Identity |
| --- | --- |
| Official reference | `maint-29`, re-fetched 2026-10-01, unchanged |
| Checkout / pin | `21776803ecd11f5fa948732c0ec66b8f325dedfc` |
| Installed oracle | OTP `29.1.1`, ERTS `17.1`, separate from source reference |
| Native runner | Windows x86_64, Visual Studio 18 x64 environment |
| Compiler / SDK | Clang/LLVM `23.1.2` |
| Quality tools | clang-tidy `22.1.8`, Lizard `1.24.0` |

The implementation did not modify the originally untracked
`references/otp/lib/stdlib/src/1.ir`; the user confirmed removing it during this
task. Grammar, corpus and pinned source hashes remain unchanged. Historical
validation records retain their original revisions and outcomes.

`patternmatch_bindings` compares 26 authored legality cases with OTP, including
wildcards/named underscores, repeated names, clause isolation, aliases, compound
patterns, head-to-guard visibility, forbidden guard assignments, RHS-first chains,
body rebinding, sibling visibility and short-circuit unsafe reads (including
unsafe precedence when sibling environments merge). It checks
source-located diagnostics through both positional and project CLI modes at
O0/O2 with specialization enabled/disabled. Failed batches preserve a sentinel and
publish no artifacts; a subsequent valid batch succeeds. Include and macro
diagnostics retain source provenance. Inspection reports stable binding identities
and unknown parameter facts.

Six complete helpers are extracted unchanged from pinned
`lib/compiler/test/match_SUITE.erl`: `gh_6516_scope1/0`, `gh_6516_scope2/0`,
`mutable_variables_1/0`, `match_right_tuple_1/1`, `force_succ_regs/2`, and `id/1`.
Their wrapper retains the upstream license and supplies only module/export
declarations. OTP accepts that wrapper; our CLI reports the expected deferred
capabilities without erroneous binding diagnostics. This is binding legality
evidence, not executable matching coverage.

The unchanged `force_succ_regs/2` and `id/1` clauses also execute from CLI-emitted
objects through the separate runtime consumer in all four optimization policies.
An authored cross-module identity wrapper and a deliberately incorrect projection
specification preserve argument behavior. Values agree with the installed OTP
oracle; existing native immediate boundaries, registration rejection, missing
runtime link failure, service failure and teardown checks remain enabled.
[Retained evidence](patternmatch-step4-evidence.json) records exact source and
generated hashes, clause text, adaptations, oracle versions and outcomes.

`semantic_bindings` retains focused invariants that cannot yet be observed through
native clause dispatch: distinct clause identities, stable reanalysis, definition/
read/check classification, absent wildcard definitions, original argument
provenance, conservative extracted/body values, sibling equality identities,
candidate failure/nonpublication, explicit success publication and injected-budget
cleanup. Step 9 still owes real same-name clause-isolation and failed-candidate
rollback execution. No earlier step's executable scope is expanded here.

| Check | Final result |
| --- | --- |
| Fresh combined Debug configure/build | Pass; compiler/runtime/testing explicitly enabled in the fresh cache |
| Full CTest | **111/111 passed**, no failures/skips; 86.79 seconds |
| Binding CLI/OTP workflow | 26 authored cases plus the unchanged helper wrapper; both CLI modes, all four optimization/specialization policies |
| Native projection/identity | Four policies, repeated execution, cross-module calls, misleading spec and existing immediate boundaries |
| Source integrity | Pinned grammar/corpus, original-suite parsing and deliberate stale-hash rejection pass |
| Existing integration coverage | Atom ownership, generated failures/retry, failed-batch nonpublication and seven-target object/layout checks pass |
| Production quality | Full Lizard and clang-tidy pass over all **191** original production commands; no threshold changes or suppressions |
| Additional checks | New invariant harness passes focused clang-tidy; C++ harness/Python driver pass Lizard; clang-format dry run and whitespace checks pass |

The initial quality findings were resolved with a compact enum, separately owned
scope-stack entries (avoiding allocating Windows map moves), and smaller pattern
child visitors. The final full gate includes the unsafe sibling-merge regression
fix; the retained evidence matches that final passing workflow.

Linux x86/ARM, Apple Silicon and native Windows 32-bit execution remain unavailable.
Foreign-object/layout checks are not native execution; no new sanitizer result is
claimed. Pattern normalization, matching, guard execution and dispatch belong to
later steps and are not implemented by this checkpoint.

Reproduce with the documented native toolchain and local dependency overrides:

```text
cmake --preset debug --fresh -DBUILD_TESTING=ON -DERLANG_AOT_BUILD_COMPILER=ON -DERLANG_AOT_BUILD_RUNTIME=ON <toolchain/dependency overrides>
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure -j 4
cmake --build build/debug --target check-quality
```

The local gate and detailed logs are under ignored `build/patternmatch-step4/`;
all fixtures, oracle drivers and test registrations are tracked.
