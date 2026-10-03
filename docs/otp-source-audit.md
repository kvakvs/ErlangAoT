# OTP source and fixture provenance â€” 2026-10-03

The repository contained 130 OTP-notice-bearing source files: two copied
preprocessor headers and 128 generated pattern/guard fixture files. The fixture
generators also extracted suite helpers and preserved source snippets in current
manifests. Compiler, runtime, ABI and example sources contained no OTP license
headers.

The copied headers and their license file were removed. Local assertion-macro and
typed-record fragments now exercise the preprocessor, with a fresh token golden.
Copied suite helpers were replaced by local binding, clause, alias, guard,
container, arithmetic, bitstring and record examples. The 222 Erlang source/include
fixtures now live under `tests/fixtures/patternmatch/fragments/`, with explicit
`corpus.json` input inventories. Compatibility helper names remain where fixed
calls use them. Valid Erlang and modified term files were formatted with the local
erlfmt checkout at `thirdparty/tools/erlfmt/`, which is ignored by Git.

Regeneration stages local inputs and records OTP observations; it never extracts
or publishes OTP source. `generated/` retains call inputs, oracle observations and
manifests, with local-source and input-inventory hashes. All nineteen corpora retain
67,634 native observations and 106 semantic rows. Normal tests need neither OTP
source nor its runtime. `fixture_sources` checks source isolation and all inventories.

OTP source, generated OTP headers, compiled oracle modules and live audit outputs
stay in ignored reference/build directories. Stored observations of local inputs
remain committed by user preference. Existing Git history and dated validation
records retain their original provenance; they describe the inputs used then.

The official `maint-29` fetch still matches the checkout and reviewed pin
`21776803ecd11f5fa948732c0ec66b8f325dedfc`. The checkout is clean. A whitespace/comment
independent comparison of 60-token windows against 4,150 OTP Erlang/include files
found only the locally generated ascending-integer stress tuple. The corresponding
comparison against 1,190 OTP C/C++ files found no overlap with production C++.
These comparisons support the explicit helper/header inventory review.

Validation: fresh combined Windows x64 Debug build; 125/125 OTP-free CTests;
explicit OTP 29.1.1 observation refresh and `--check` for all nineteen corpora;
optional upstream audit; grammar reduction audit with no ordinary rows pending;
all ten real-source parser corpus entries. Logs and comparison inventories remain
under ignored `build/otp-cleanup-*` paths.

The required `check-quality` target passes Lizard and clang-tidy for all 258
production translation units without threshold changes or new suppressions.
