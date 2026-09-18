# Parser implementation and validation

Baseline: OTP 29.1 (`751f87b703fe5948607d08e82599ce644b772e76`).
Implementation follows [.agents/02-parser.md](../.agents/02-parser.md).

## Phase I progress

| Step | Behavior | Evidence |
| --- | --- | --- |
| 1 | Pinned grammar/action inventory; raw parser, epp, and lint reference records | `tests/fixtures/parser/`, `parser_reference`, `parser_oracle` |

Step 1 establishes the reference harness; it does not implement the full parser.
Offline testing checks native scanning and reference integrity. Live tests compare
OTP parse/epp/lint output against checked-in records, with precise version skips.
See the [corpus README](../tests/fixtures/parser/README.md) for normalization,
provenance, upstream cases, and limits. Syntax acceptance is separate from lint,
record expansion, parse transforms, and Core lowering.

Step 1 validation (macOS arm64): fresh C++23 full configure/build; all 12 CTests
passed, including live OTP 29.1. Missing and OTP 28 oracle paths explicitly skip.
The inventory contains 423 productions (including explicit SSA exclusions) and
14 ordinary builder families. No grammar coverage is inferred from a seed alone.
Fresh full-build Lizard and clang-tidy both passed before the step 1 commit.
