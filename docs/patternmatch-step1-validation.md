# Pattern/guard step 1 validation — 2026-10-01

Completed **Fix the semantic matrix and OTP evidence** from
[the patternmatch plan](../.agents/11-plan.md#completed-patternmatch). Steps 2–20 remain pending;
this change does not enable executable patterns/guards or alter the runtime ABI.
The [semantic matrix](patternmatch-matrix.md) and
[retained evidence](patternmatch-step1-evidence.json) define the measured boundary.

| Dependency / provenance | Exact identity |
| --- | --- |
| Official source branch | `maint-29`, fetched again on 2026-10-01; unchanged |
| Source checkout and pin | `21776803ecd11f5fa948732c0ec66b8f325dedfc` |
| Installed execution oracle | OTP `29.1.1`, ERTS `17.1` |
| Native host | Windows x86_64, Visual Studio 18 x64 environment |
| Compiler SDK | Clang/LLVM `23.1.2`, existing local SDK |
| Analysis tools | clang-tidy `22.1.8`, Lizard `1.24.0` |
| Source identity policy | SHA-256 of bytes with only CRLF normalized to LF |

The existing ten-file parser corpus and grammar evidence are unchanged and passed
their tests. The reference checkout retains only its original untracked
`lib/stdlib/src/1.ir`; no reference work was discarded. Historical validation
documents and their revisions remain unchanged.

| Check | Result / scope |
| --- | --- |
| Fresh combined Debug configuration and build | Pass; compiler/runtime ON, `BUILD_TESTING=ON` |
| Full CTest | **104/104 passed**, zero failures/skips (97.22 seconds on this host) |
| `patternmatch_evidence` | Pass (16.72 seconds); evidence below |
| Source/fixture provenance | Both reviewed manifests pass; deliberately stale source hash rejected before source processing |
| Guard catalog | All **81** explicit BIF/type-test/operator category/name/arity rows match pinned `erl_internal.erl`; each has owner/rejection policy |
| Original OTP suites | `guard_SUITE`, `match_SUITE`, `trycatch_SUITE` preprocess/parse with actual includes; no suite execution claimed |
| Authored OTP acceptance | **14** modules: 13 syntax accepted, one syntax rejected; two semantic accepted, 12 semantic rejected with expected diagnostic families |
| Authored OTP execution | **40** expected observations, including selected clauses and stable error class/reason |
| Unchanged OTP helpers | `bif_SUITE:first/2`, `guard_SUITE:id/1`; exact clauses, source hashes, license notices and wrapper declarations retained |
| Public CLI/native baseline | O0/O2, default and disabled specialization policy, twice per mode; separate runtime consumer, integer/immediate identity boundaries and cross-module calls |
| Existing regression workflows | Full suite retains misleading specs, positional/project modes, failed-batch nonpublication, failure recovery, ABI rejection and missing-runtime link failure |
| `check-quality` | Full Lizard and clang-tidy pass, unchanged thresholds/suppressions; all **184** original production compilation commands retained verbatim |
| New Python driver complexity | Separate Lizard check passes; maximum function CCN **4**, threshold **10** |

O0 skips specialization internally; its default and `--no-type-specialization`
runs exercise both CLI policies, not specialized clones. These source helpers
remain generic at O2 too: they require no removable type checks. Pattern/guard
seed execution is OTP-only; it must not be counted as native matching coverage.

The full suite also passes the existing seven-target foreign-object inspection
and 32-bit layout checks. Those are not native execution evidence. Linux x86/ARM,
Apple Silicon and native Windows 32-bit runners remain unavailable for this step.
No new sanitizer run is claimed.

Reproduce in the configured native toolchain environment:

```text
cmake --preset debug --fresh -DBUILD_TESTING=ON -DERLANG_AOT_BUILD_COMPILER=ON -DERLANG_AOT_BUILD_RUNTIME=ON <documented local dependency/toolchain overrides>
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
cmake --build build/debug --target check-quality
```

Use [the existing compilation setup](compile.md) for local SDK/toolchain overrides;
this Windows run selects `C:/Program Files/Erlang OTP/erts-17.1/bin/escript.exe`
because the installation's generic escript launcher is broken. Build-local logs
are under `build/patternmatch-step1/`; per-case evidence is under
`build/debug/tests/compiler/codegen/patternmatch/Debug/`. The tracked JSON snapshot
preserves source/helper hashes, wrapper adaptations, installed oracle version and
all acceptance/results without depending on those ignored build directories.
