# Pattern/guard step 8 validation — 2026-10-02

Implements **Lower guard grouping and short-circuit behavior** from
[the plan](../.agents/11-plan.md#completed-patternmatch); see [the contract](guard-control-flow.md).
Official maint-29 was fetched at the start of steps 6–8; upstream, clean checkout
and pin remain `21776803ecd11f5fa948732c0ec66b8f325dedfc`. Grammar/corpus hashes
are unchanged; `andor_SUITE.erl` is additionally pinned. Installed oracle:
OTP 29.1.1 / ERTS 17.1. Native runner: Windows x64, Visual Studio 18 environment,
Clang/LLVM 23.1.2. Quality: Lizard 1.24.0, clang-tidy 22.1.8. Historical evidence
retains its original revisions and outcomes.

`patternmatch_booleans` compares **2,075 source calls** with OTP through unmodified
public CLI objects and a separately built runtime-only consumer. O0/O2 with
specialization on/off use both positional and project compilation. Each native
consumer repeats the complete call stream, checking channel cleanup after every
invocation and explicit runtime teardown.

Cases cover comma/semicolon grouping; strict `and/or/xor/not`; lazy term-valued
joins; canonical-true guard boundaries; non-boolean operands/results; nested
grouping; reached `badarg` versus semicolon recovery; skipped bad arguments;
local/remote generated failures; wrong specs; shadowed body calls and qualified
services; repeated/alias head bindings read across alternatives; and head-first
execution. Invalid lazy left operands preserve exact `{badarg, Value}` payloads
for integers, Unicode/other atoms and canonical empties. The oracle and native
protocol encode structured payloads by spelling/value rather than stack or raw ID.

Licensed adaptations retain complete first guard/body clauses from
`guard_SUITE:csemi4_orelse_{a,b,c,d}/4`; their fallback clauses remain explicitly
deferred to step 9. Scalar grouping tests adapt `semicolon/1` and `comma/1` from
if/Common Test wrappers to single function clauses. Lazy/strict tests adapt
`andor_SUITE:t_andalso/1` and `t_orelse/1`, replacing unavailable exit operands
with admitted `hd([])` or real generated clause failures. Floats, heap values,
assignment-dependent and unrelated harness behavior are excluded explicitly.
[Evidence](patternmatch-step8-evidence.json) records exact source/fixture/clause/
wrapper hashes, declarations and adaptations; notices are retained.

Public stress cases execute 129 semicolon alternatives, 129 comma tests and
96 nested lazy expressions on the complete admitted domain. Before/after IR and
type modes succeed; unoptimized IR retains separate RHS/alternative/comma blocks
and target-word PHIs. Both 32/64-bit objects and verified IR pass separately from
native execution. The existing full-suite negative cases still reject illegal or
unavailable calls/assignments behind constant lazy operands in both CLI modes and
four policies, preserving prior artifacts and later successful compilation.

Four injected-service workflows rerun exact allocation/resource/ownership/
unavailable/internal failures against semicolon fallback, nested orelse, strict
operators and skipped RHS predicates. Failures stop execution before result use;
skipped predicates enter no service; cleanup and retry pass. A failed head enters
no guard despite an unconditional later alternative. Source fault objects retain
their labeled private service seam; differential objects remain unmodified.

Initial comparisons exposed OTP's structured lazy badarg reason; the implementation
now retains it rather than normalizing it away. Analyzer findings were resolved
with named guard continuations and LLVM's SSA API. A debugger identified the SSA
API's requirement for a terminated merge during construction; a temporary
terminator supplies it and is removed before final verification. No suppressions,
relaxed thresholds or excluded production commands were used.

Final fresh combined Debug configure/build: **120/120 CTests**, zero skips, full
Lizard and clang-tidy over **207 production units**, formatting and whitespace
checks pass. Local gate/logs: ignored `build/patternmatch-step8/`.

Linux x86/ARM, Apple Silicon and native Windows 32-bit runners remain unavailable.
Foreign objects are inspection evidence only; no new sanitizer run is claimed.
Steps 6–8 are complete and separately committed; work stops here. Ordered dispatch,
fallback-clause reruns and all later plan steps remain open.
