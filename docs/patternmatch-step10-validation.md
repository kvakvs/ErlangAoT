# Pattern/guard step 10 validation — 2026-10-02

Implements **Integrate body matches and sequences (F16 slice)** from
[the plan](../.agents/10-patternmatch.md); see [the contract](body-matches.md).
The task-start official maint-29 fetch matches the clean checkout and pin
`21776803ecd11f5fa948732c0ec66b8f325dedfc`. Grammar/corpus/source hashes are
unchanged. Installed oracle: OTP 29.1.1 / ERTS 17.1. Native runner: Windows x64,
Visual Studio 18 environment, Clang/LLVM 23.1.2; Lizard 1.24.0 and clang-tidy 22.1.8.
Historical validation records keep their original inputs and results.

`patternmatch_sequences` compares **1,666 calls** with OTP through public CLI
objects and the separate runtime-only consumer. O0/O2 with specialization on/off
cover positional and project modes; native streams run twice with channel-cleanup
checks after every invocation and explicit runtime teardown. Both IR modes and
type inspection succeed for every admitted helper.

Cases cover ordered sequences, new bindings, exact checks against existing names,
head parameters used only in match constraints, RHS-created bindings, right-first
chains, parenthesized compound aliases, wildcards and ordinary underscore names,
folded pattern constants, atom literals only present in a pattern, empty values,
matches inside calls and sibling argument constraints. Incorrect specs preserve
behavior. Stress helpers execute 128 ordered matches and a 128-name chain.

The licensed `match_SUITE:mutable_variables_1/0` adaptation retains every
assignment and its guaranteed conflicting match; only its unreachable tuple
construction is replaced by Result. `in_call/1` adapts mac_c/1's chained bindings
inside calls to immediate inputs; `id/1` is unchanged. Authored helpers retain
all matching operations under test. [Evidence](patternmatch-step10-evidence.json)
records source/retained-helper text, adaptations, exports and wrapper hashes.

Mismatch streams compare exact `badmatch` payloads for integers, atoms (including
Unicode) and canonical empties. A following hd([]) distinguishes successful
matching from an early failed match; nested callers preserve the original error.
A selected body's error never retries a later clause. The four native service
fault workflows count a real predicate call to prove single RHS evaluation and
skipping later body work, verify owned atom payloads, inject exact infrastructure
statuses and confirm later successful retry.

The full semantic suites retain illegal/unsafe binding, deferred compound-pattern,
recursion, failed-batch nonpublication and recovery cases. Reviewed fixture
capability expectations now admit immediate body matches and classify remaining
chain-result tuple construction as heap expressions; the pattern fixture manifest
was updated for those explicit expectation changes.

Fresh combined Debug configure/build: **122/122 CTests**, zero skips (107.20 s).
Full Lizard and clang-tidy pass over **209 production units**; formatting and whitespace checks pass. Call lowering was separated into a small helper and pattern-root capacity is reserved explicitly to satisfy the unchanged quality checks.
Local logs: ignored `build/patternmatch-step10/`.

Other native hosts (Linux x86/ARM, Apple Silicon, Windows 32-bit) remain unavailable.
Existing foreign-object/layout checks remain distinct from native execution;
no new sanitizer result is claimed. Steps 11–20 remain open.
