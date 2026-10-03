# Pattern/guard step 6 validation — 2026-10-02

Implements **Implement immediate equality and matching (F12 slice)** from
[the plan](../.agents/11-plan.md#completed-patternmatch); see [the contract](immediate-matching.md).
Official `maint-29` was fetched on 2026-10-02; upstream, clean checkout and pin
remain `21776803ecd11f5fa948732c0ec66b8f325dedfc`. Grammar/corpus/evidence hashes
are unchanged. Installed oracle: OTP 29.1.1 / ERTS 17.1. Native runner: Windows
x64, Visual Studio 18 environment, Clang/LLVM 23.1.2; quality: Lizard 1.24.0 and
clang-tidy 22.1.8. Historical records retain their original revisions/outcomes.

`patternmatch_immediate` compares 34 complete source invocations with OTP and the
separate runtime-only consumer in four O0/O2 and specialization policies, using
both positional and project compilation. Cases exercise repeated integers/owned
atoms/empty values, aliases, wildcards, character/literal mismatch, normalized
arithmetic, native integer endpoints, misleading specs and nested local/remote
failure followed by successful retry. Mismatch observes error `function_clause`.

Licensed adaptations retain immediate constraints from
`match_SUITE:char_alias_1/1`, `str_alias_1/1` and `multiple_aliases_2/1`. Later and
fallback clauses are removed until step 9; the empty alias returns `[]` and the
multiple alias returns `C` instead of its construction-dependent tuple. Constraints
under test remain intact. [Evidence](patternmatch-step6-evidence.json) records
source/fixture/wrapper hashes, declarations, adaptations and exact widths.

32/64-bit endpoint patterns emit objects and verified inspection IR; each width
rejects its first out-of-range integer without publication. These are layout and
foreign-object checks, not 32-bit execution. The native consumer admits only real
runtime values and separately rejects forged/foreign equality operands, checks
channel cleanup and retry, and tears down explicitly. A focused tiny-budget test
proves no partial private plan escapes and construction subsequently recovers.

The full regression suite preserves OTP evidence, source limits, semantic errors,
both CLI modes, failed-batch nonpublication, direct calls, identity/projection,
ownership and seven-target object checks. Initial failures in obsolete capability
expectations were updated to nonempty deferred examples. Unconditional heads were
kept compact to preserve existing IR/size regressions. Complexity findings were
resolved with smaller helpers and named match options, without suppressions.

Final fresh combined Debug configure/build, **114/114 CTests**, zero skips, full
Lizard and clang-tidy over **199 production units**, clang-format and whitespace
checks pass. Local gate/logs: ignored `build/patternmatch-step6/`.
The first full clang-tidy invocation crashed in unchanged frontend files without
project findings. A complete unchanged rerun of `check-quality` passed; both logs
are retained locally. No checks or production commands were excluded.

Linux x86/ARM, Apple Silicon and native Windows 32-bit runners remain unavailable.
No new sanitizer result is claimed. Guards, ordered dispatch and all later
pattern representations remain assigned to steps 7–20.
