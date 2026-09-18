# Parser implementation and validation

Baseline: OTP 29.1 (`751f87b703fe5948607d08e82599ce644b772e76`).
Implementation follows [.agents/02-parser.md](../.agents/02-parser.md).

## Phase I progress

| Step | Behavior | Evidence |
| --- | --- | --- |
| 1 | Pinned grammar/action inventory; raw parser, epp, and lint reference records | `tests/fixtures/parser/`, `parser_reference`, `parser_oracle` |
| 2 | Shared bounded cursor, syntax diagnostics and contextual operators | `parser_tokens`, unchanged preprocessing/scanner records |
| 3 | Typed flat AST ownership, generations, transactions and source origins | `parser_ast`, ASan/UBSan |
| 4 | Minimal expanded-form parser, immutable features, limits and recovery | `parser_forms`, `parser_phase1_golden`, `parser_phase1_oracle` |

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

## Typed AST foundation (step 3)

Step 2 commit: `3bf8682`.
Public headers under `compiler/ast/` expose a move-only `ast::Module`, distinct
expression/form/pattern/type handle categories, closed payload variants, and const
visiting/access APIs. Only literal/variable expressions and module/file attributes
plus an explicitly named `ZeroArgumentFunction` exist at this stage. This is syntax,
not validated binding/type information; full node families arrive with their steps.

Private `ast::Builder` opens one RAII transaction per form. Only its committed root
becomes visible in the source-order root list. Rollback destroys appended nodes and
origins iteratively; generations never rewind, so reused slots reject stale handles.
IDs retain a small owner identity token, not the AST. Module moves retain identity;
foreign IDs and moved-from module access throw controlled C++ exceptions. Consumers
must not mix owners or retain node references across builder mutation. After
`finish()`, the owner is read-only and references remain valid for its lifetime.

Each node carries a half-open range into a per-form table of owned token origins.
Physical spelling, logical invocation position, and ordered macro/include traces
remain separate; ranges never pretend that unrelated source buffers are contiguous.
An explicit EOF origin supports empty/synthetic extents without indexing tokens.
The builder validates ranges, anchors, nonempty function bodies, and child ownership.
Pattern/type ID types reserve category safety without speculative node storage.

Native tests cover 8,192-node arena growth, moves, exhaustive typed visiting,
rollback/recycled handles, cross-owner references, invalid construction, empty
origins, and macro values originating in an included header after all preprocessing
objects have been destroyed. Initial quality findings led to reference-based ID
access, unambiguous ID construction, and non-throwing rollback; no suppressions or
threshold changes were introduced.

Step 3 validation (macOS arm64): fresh full C++23 configure/build and all 14 CTests
passed after fixes. ASan/UBSan passed AST, token, directive-form and preprocessing
semantic tests. The full Lizard/clang-tidy gate passed without relaxed thresholds.

## Expanded-form parsing (step 4)

Step 3 commit: `3b5406b`.
`ParserSession::parse_form(tokens, eof, features)` parses one raw expanded token
range, including its lexical dot. `consume(event)` accepts semantic preprocessing
forms/diagnostics; directives and missing semantic feature context produce explicit
contract errors. `parse_module(preprocessor)` drives the normal pipeline. Results
own completed AST forms and diagnostics, with a latched `failed` flag. Warnings
alone succeed. Ordinary syntax failures roll back the entire form and parsing
continues at the next preprocessor boundary. Empty input uses the explicit EOF
anchor; trailing tokens and missing dots cannot be silently accepted.

Current grammar: `-module(atom).`, `-file("name", Integer).`, and zero-argument
functions with one atom/integer/float/character/string literal body. Integers retain
arbitrary precision, floats retain binary64 bits, and characters/strings retain
Unicode values. File attributes include preprocessor-generated include transitions.
Other syntax reports `unsupported_syntax`; it is not claimed to be invalid Erlang.
There is no parse-check CLI yet (plan step 17), no variable binding/guard/type checks,
no transform execution, and no code generation. Raw-token feature state can be
unspecified; semantic preprocessing always supplies an immutable snapshot.

`ast::Module::features(form)` exposes the context at form emission, and `features()`
exposes the session's final enabled set at normal EOF. Earlier snapshots never
change when later directives/includes alter features. Sessions share no mutable
state. A resource-limited result is failed and records feature state at stopping,
not a claim that the remaining input was consumed.

Default limits: 1,000,000 tokens per form, 4,000,000 total input tokens, 1,000,000 AST
expression/form nodes, and 1,000 diagnostics plus one reserved exhaustion message.
Resource errors stop further parser work; failed forms consume input-work budget
but retain no AST nodes/origin tables. Recursive depth limits will accompany the
recursive grammar; Phase I itself parses no recursive constructs.

The closed Phase I dump projection compares actual native ASTs against three pinned
OTP records (`minimal`, `expanded`, and `phase1`), both offline and live. Unknown
payloads fail the projection. Shared test encoders preserve Unicode and exact float
bits across scanner/preprocessor/AST dumps. The mixed-newline include corpus found
and now protects a preprocessor correction: immediate LF after an include dot
advances its implicit return file line; a space, comment, or CR follows OTP's
single-character dot-scanning behavior. File attributes remain in the comparison.
See `docs/preprocessor.md` for that narrowly scoped behavior change.

Step 4 validation (macOS arm64): all 17 CTests passed in freshly configured C++23,
C++26, and ASan/UBSan builds, including the live OTP 29.1 suites and actual native
AST parity. Runtime-only configure/build succeeded with nonexistent Boost paths.
The final full Lizard/clang-tidy gate passed at unchanged thresholds. Linux and
Windows execution remain unvalidated; they are not implied by these host results.
