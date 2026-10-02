# Erlang/OTP source reference

The source reference follows the official Erlang/OTP `maint-29` branch. The current
reviewed pin is `21776803ecd11f5fa948732c0ec66b8f325dedfc`, fetched on 2026-09-28
(upstream commit dated 2026-09-22). The machine-readable revision and branch live
in [`references/otp-pin.cmake`](../references/otp-pin.cmake).

Before future OTP-dependent work, fetch `maint-29` and refresh the pin if its head
has advanced. Each refresh records an exact commit so offline validation stays
reproducible. Configuration and tests never fetch or advance the reference.
The installed OTP used for live oracle tests is a separate dependency.

1. Check `git -C references/otp status --short` and preserve any local work. Fetch
   the official branch with
   `git -C references/otp fetch https://github.com/erlang/otp.git refs/heads/maint-29:refs/remotes/origin/maint-29`.
   Inspect `git -C references/otp log -1 origin/maint-29` and review changes since
   the recorded pin, especially grammar, scanner, preprocessor and corpus inputs.
2. Advance the clean local `maint-29` branch with `git merge --ff-only origin/maint-29`
   from the checkout. On first setup use `git switch --create maint-29 --track origin/maint-29`.
   Use LF checkout bytes (`core.autocrlf=false`) so checksums match on every host;
   set this at clone time for a new checkout. For an existing Windows checkout,
   normalize only the manifest-listed text files and `erl_parse.yrl` to LF, and
   verify Git still reports no source changes. Preserve upstream binary fixtures.
3. Generate the compiler header from that same revision, from the project root:
   `perl references/otp/erts/emulator/utils/beam_makeops -compiler -outdir references/otp/lib/compiler/src references/otp/lib/compiler/src/genop.tab`.
   On Windows normalize only the generated `beam_opcodes.hrl` from CRLF to LF.
4. Update the pin and verify every path in `tests/fixtures/parser/phase6/otp.tsv`.
   Refresh hashes only after reviewing actual upstream changes. If `erl_parse.yrl`
   changes, review `tests/fixtures/parser/grammar.tsv`, its authored fixtures and
   `phase6/coverage.tsv`; every ordinary production still needs a measured witness.
   Historical oracle records keep their original provenance.
5. Configure with `ERLANG_AOT_OTP_SOURCE_ROOT` pointing to the refreshed checkout.
   Run `parser_coverage`, `parser_corpus`, parser historical/integrity tests and the
   affected frontend/oracle tests. Update current reference documentation and
   record the exact revision, outcomes and any remaining failures.

The 2026-09-28 refresh has identical grammar and all ten corpus entries compared
with the previous `OTP-29.1` pin. Their existing hashes and reduction witnesses
remain applicable; no expected output was regenerated merely to pass validation.

Validation for this refresh: `parser_coverage` and `parser_corpus` pass, and the old
pin is rejected with the required branch/revision diagnostic. Final Windows Debug
CTest passes 74/75, with only the existing `parser_hardening` stack overflow.
The freshly configured full build and Lizard pass; the full clang-tidy gate still
reports the pre-existing Windows exception-escape and Boost analyzer findings.
The checkout is clean; relative documentation links and `git diff --check` pass.
No compiler-plan step beyond 14 or clean commit is claimed by this reference update.

## Pattern/guard step 1 check (2026-10-01)

Re-fetched official `maint-29`; the head, checkout and pin still match
`21776803ecd11f5fa948732c0ec66b8f325dedfc`. Grammar and the ten original corpus
hashes remain unchanged; full parser coverage/corpus tests pass. The original
untracked `lib/stdlib/src/1.ir` is preserved. No reference checkout files were
rewritten. The separate installed oracle is OTP 29.1.1 / ERTS 17.1.

[Pattern/guard evidence](patternmatch-step1-validation.md) adds exact source and
fixture hashes, original-suite parsing, separate syntax/semantic acceptance and
unchanged helper native execution. Its stale-hash rejection passes. The fresh
combined Windows x64 Debug gate passes 104/104 tests and full Lizard/clang-tidy.
The older refresh record above retains its original results and revision.

## Pattern/guard step 2 check (2026-10-01)

Re-fetched official `maint-29`; upstream, checkout and pin still match
`21776803ecd11f5fa948732c0ec66b8f325dedfc`. Existing grammar/corpus and step-1
source hashes pass unchanged. The original untracked `lib/stdlib/src/1.ir` remains.
The separate installed oracle is still OTP 29.1.1 / ERTS 17.1.
[Step-2 validation](patternmatch-step2-validation.md) records failure-transport
execution, the 108/108 combined Windows Debug result and full quality pass.

## Pattern/guard step 3 check (2026-10-01)

Re-fetched official `maint-29`; upstream, checkout and pin remain
`21776803ecd11f5fa948732c0ec66b8f325dedfc`. Grammar/corpus and step-1 source hashes
pass unchanged; untracked `lib/stdlib/src/1.ir` is preserved. Installed oracle remains
OTP 29.1.1 / ERTS 17.1. [Step-3 validation](patternmatch-step3-validation.md) records
licensed atom-leaf adaptations, four native policies, ownership/limit/fault tests,
109/109 combined Windows Debug tests and full Lizard/clang-tidy over 189 production
units. Earlier validation records keep their original revisions and outcomes.

## Pattern/guard step 4 check (2026-10-01)

Re-fetched official `maint-29`; upstream, checkout and pin remain
`21776803ecd11f5fa948732c0ec66b8f325dedfc`. Grammar/corpus and pinned source hashes
pass unchanged. The user confirmed removing the previously untracked
`lib/stdlib/src/1.ir` during this task; the implementation did not modify it. Installed oracle remains
OTP 29.1.1 / ERTS 17.1. [Step-4 validation](patternmatch-step4-validation.md) records
26 scoped-binding legality cases, six unchanged match_SUITE helpers, native
identity/projection in four policies, 111/111 combined Windows Debug tests and full
Lizard/clang-tidy over 191 production units. Clause-isolation/rollback execution
remains assigned to step 9. Earlier records retain their original evidence.

## Pattern/guard step 5 check (2026-10-02)

Fetched official `maint-29` at task start on 2026-10-01; upstream, checkout and pin
remain `21776803ecd11f5fa948732c0ec66b8f325dedfc`. The checkout is clean. Grammar,
corpus and existing evidence hashes are unchanged; step 5 separately pins
`erl_bits.erl` for binary modifier rules. Installed oracle remains OTP 29.1.1 /
ERTS 17.1. [Step-5 validation](patternmatch-step5-validation.md) records 92 authored
legality cases, unchanged/adapted suite helpers, both CLI modes/four policies,
bounded normalization/rollback, 113/113 combined Windows Debug tests and full
Lizard/clang-tidy over 196 production units. Matching/guard execution remains
pending. Historical records retain their original revisions and outcomes.

## Pattern/guard step 6 check (2026-10-02)

Fetched official maint-29; upstream, clean checkout and pin remain `21776803ecd11f5fa948732c0ec66b8f325dedfc`. Grammar/corpus/source hashes are unchanged; installed oracle remains OTP 29.1.1 / ERTS 17.1. [Step-6 validation](patternmatch-step6-validation.md) records immediate source matching, shared checked equality, 34 OTP/native calls in four policies, both-width endpoint objects, 114/114 Windows Debug tests and full Lizard/clang-tidy. Historical evidence is preserved.
