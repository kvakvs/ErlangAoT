# Pattern matching step 17 validation

Completed 2026-10-03 on Windows x64. Official maint-29 was fetched on
2026-10-02 at step start; upstream, clean checkout and pin remained
`21776803ecd11f5fa948732c0ec66b8f325dedfc`. The installed oracle was OTP
29.1.1 / ERTS 17.1. LLVM SDK 23.1.2, Lizard 1.24.0 and clang-tidy 22.1.8
were used. Historical validation records retain their original provenance.

| Evidence | Result |
| --- | --- |
| Fresh combined Debug configure/build | Pass; compiler/runtime/testing ON, OTP audits OFF, deliberately absent OTP paths |
| Full CTest suite | 121/121, zero skips, 333.06 s |
| Lizard | Pass; CCN limit 10 unchanged |
| clang-tidy | Pass; all 257 production units |
| Project-owned record corpus | 1,025 outcomes in four O0/O2/specialization policies, positional/project modes |
| Semantic record cases | 29 OTP-backed cases; located rejection, nonpublication and recovery |
| Explicit regeneration check | Stored record inputs/results reproduced byte for byte |
| Original suite/data parsing | Both pass with real includes/features; syntax evidence only |
| Foreign objects | Two modules each for i686 Windows, x64 Windows and AArch64 Linux; headers/width/service symbols inspected |
| Formatting / whitespace | clang-format dry run and git diff --check pass |

The admitted tuple slice resolves included declarations, fields, defaults and
source order. Construction uses declaration order and captures each expanded
initializer immediately. Patterns constrain the tag/arity and supplied fields,
including wildcard expansion and repeated-name equality. Access shares checked
tuple services and retains owned `{badrecord,Value}` payloads in bodies; guards
reject wrong shapes. Both forms of `is_record/2` and the admitted-domain
`is_record/3` rules agree with OTP, including dynamic tags, atom third arguments,
zero/negative sizes, boxed-size `badarg`, qualified and legacy tests.

The corpus identifies adaptations of `record_SUITE:errors/1`, `eval_once/1` and
`nested_access/1`. Update portions and the closure/process-dictionary counter
remain deferred. IR counts supplement the golden values to establish one call
per accessed operand, two default calls, and three separately expanded wildcard
calls. The complete suite and `record_access_in_guards.erl` are parsed rather
than executed as Common Test modules.

Four native fault policies cover record construction in guards/bodies and checked
access. Injected ownership/resource/internal failures remain infrastructure
failures, with empty roots/channel after cleanup and successful retry. A retained
badrecord tuple remains valid after 32 subsequent allocations. The public CLI
rejects 2,000 omitted fields expanded at 600 sites at the shared semantic work
ceiling without publishing artifacts. Included-field diagnostics retain their
physical header location.

OTP 29.1.1 accepts a huge literal `is_record/3` guard during lint but its loader
rejects the resulting `i_is_tagged_tuple` instruction. That case has retained
semantic acceptance evidence and working dynamic-BIF boundary coverage; successful
native-versus-OTP execution of that literal guard is not claimed.

Logs are retained under `build/patternmatch-step17/`. Linux, Apple Silicon and
32-bit native runners remain unavailable; no new sanitizer run is claimed.
See [record expansion](record-matching.md) and
[retained evidence](patternmatch-step17-evidence.json). Steps 18–20 remain open.
