# Pattern/guard step 5 validation — 2026-10-02

Implements **Validate and normalize pattern semantics** from
[the plan](../.agents/10-patternmatch.md). The
[semantic contract](pattern-semantics.md) documents normalized structure,
scope rules, embedded-expression legality, resource budgets and deferred owners.
Steps 6–20 remain pending.

| Dependency / provenance | Identity |
| --- | --- |
| Official reference | `maint-29`, fetched at task start on 2026-10-01, unchanged |
| Checkout / pin | `21776803ecd11f5fa948732c0ec66b8f325dedfc` |
| Installed oracle | OTP `29.1.1`, ERTS `17.1`, independent of source reference |
| Native runner | Windows x86_64, Visual Studio 18 x64 environment |
| Compiler / SDK | Clang/LLVM `23.1.2` |
| Quality tools | clang-tidy `22.1.8`, Lizard `1.24.0` |

The reference checkout is clean. Existing grammar/corpus and source manifests
retain their original hashes. A separate `pattern-otp.tsv` records the additional
`erl_bits.erl` evidence for modifier/default rules; `pattern-fixtures.tsv` pins the
new authored cases. Historical validation records are unchanged.

`patternmatch_patterns` compares **92 authored modules: 38 accepted and 54 rejected**
with the installed OTP compiler, retaining exact lint categories independently of
our human diagnostics. Cases cover constant arithmetic and failures, grouping,
aliases, strings/list prefixes, nested invalid expressions, map key expressions,
local/qualified/imported/suppressed calls, map association legality, incoming and
sibling bindings, binary segment order, size scopes and modifier constraints.
Warnings about impossible matches count as accepted, not as semantic rejection.

The CLI checks all rejected/deferred modules in positional and project batches at
O0/O2 with specialization enabled/disabled. Original source coordinates are
checked for semantic diagnostics. Failed batches preserve a sentinel, publish no
artifacts and permit a later successful identity batch. Accepted deferred forms
are checked for capability diagnostics without erroneous pattern/binding errors.
Public source tests reject excessive grammar nesting, excessive constant digits,
oversized shifts and wide arithmetic patterns that exhaust the shared work budget.

The workflow retains six complete unchanged `match_SUITE` helpers from step 4
and the complete unchanged `bs_size_expr_SUITE:do_basic_1/1` clauses. Adaptations
of `map_SUITE:t_key_expressions/1` move its key expressions into body matches with
explicit incoming parameters, removing Common Test/closure/case scaffolding.
The `element/2` adaptation retains its dependent binary value pattern. Other
literal value constraints become captures; failing division is specialized to
`1 div 0`. Tuple arithmetic, binary-valued keys and key-failure legality remain
covered. Notices, declarations, full helper clauses, source and
generated hashes, adaptations and oracle outcomes are retained in
[the evidence](patternmatch-step5-evidence.json). Original `map_SUITE.erl` and
`bs_size_expr_SUITE.erl` also parse with real include/application paths and feature
flags. Suite parsing and lint agreement are not native matching execution.

`semantic_patterns` checks owned folded integer/float values, source anchors,
group removal, alias/expression-match distinction, module-wide rollback after
ordinary errors and injected budget exhaustion, and 12,000-level legal/illegal
synthetic patterns through both restricted and permissive pattern wrappers.
These private invariants cannot yet be observed through executable matching.
Existing native identity/projection, misleading-spec, atom, failure/retry and
foreign-object/layout workflows remain required by the full suite.

| Check | Final result |
| --- | --- |
| Fresh combined Debug configure/build | Pass; compiler/runtime/testing enabled |
| Full CTest | **113/113 passed**, zero failures/skips; 94.20 seconds |
| New CLI/OTP workflow | 92 authored modules plus unchanged match/binary helper modules; both CLI modes and all four policies |
| Normalization invariants | Both syntax categories, 12,000-level traversal, owned constants/source anchors and full rollback pass |
| Existing native/integration workflows | Identity/projection, misleading specs, atoms, failure/retry, nonpublication and seven-target object/layout checks pass |
| Source integrity | Grammar/corpus, pinned hashes, original suite parsing and stale-hash rejection pass |
| Production quality | Full Lizard and clang-tidy pass over **196** production translation units; no relaxed thresholds or suppressions |
| Additional checks | New C++/Python test code passes Lizard; clang-format dry run and whitespace checks pass |

The test-driver refactor and retained map-key/binary adaptation were verified by a final focused rerun
(9.09 seconds); the retained evidence comes from that passing run. Initial
quality findings were fixed with small scope/metadata helpers, const-reference
parameters, checked optionals and in-place modifier state, avoiding allocating
Windows map moves. No implementation changes followed the fresh passing gate.

Linux x86/ARM, Apple Silicon and native Windows 32-bit execution are unavailable
on this runner. Foreign-object/layout tests are not native execution; no new
sanitizer result is claimed. Records remain recognized syntax until step 17's
expansion/field/layout validation; matching, guards and dispatch remain gated.

Reproduction uses the documented native toolchain and local dependency overrides:

```text
cmake --preset debug --fresh -DBUILD_TESTING=ON -DERLANG_AOT_BUILD_COMPILER=ON -DERLANG_AOT_BUILD_RUNTIME=ON <toolchain/dependency overrides>
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure -j 4
cmake --build build/debug --target check-quality
```

Detailed local logs and the gate wrapper are under ignored `build/patternmatch-step5/`.
