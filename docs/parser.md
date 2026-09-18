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

## Shared token mechanics (step 2)

Step 1 commit: `bd11eeb`.
`src/parsing/` now owns bounded lookahead/consume/checkpoint/restore, syntax matching,
token diagnostic construction, and infix precedence/associativity by grammar context.
Directive parsing retains symbol-only delimiters and its existing errors; condition
parsing retains its restricted operator set and normalization/evaluation behavior.
EOF anchors are supplied explicitly, and maximum-size lookahead cannot overflow.
Quoted atoms are never operators or keywords. The lexical terminal audit required
no language changes; all existing scanner/preprocessor records remain unchanged.
The Boost character rules and token-iterator rejection probe remain intact.

Step 2 validation (macOS arm64): fresh full C++23 build; all 13 CTests passed,
including native token checks and live/offline preprocessing comparisons. Full
Lizard and clang-tidy passed at the unchanged thresholds before commit.
