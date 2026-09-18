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

## Phase II: literals and aggregates (step 5)

Step 4 commit: `d84bcb2`. Expressions now include variables (including `_`), tuples,
proper/improper lists, explicit grouping, adjacent string concatenation, and OTP
29.1 sigils. Existing decoded lexer values are reused unchanged. `s`/`S` sigils
produce strings; the empty prefix and `b`/`B` produce a typed UTF-8 binary-sigil
literal pending general binary syntax in step 9. Unknown prefixes and nonempty
suffixes fail parsing, as does concatenating a sigil with an ordinary string.

Aggregate children remain checked expression handles in flat arenas. Lists retain
an ordered element vector and optional explicit tail; groups preserve parentheses
and token extents. The private comparison projection removes groups and normalizes
list spines to OTP's cons/nil representation. Builder validation rejects foreign
children and headless tails. Parser nesting defaults to 256 recursive expression
calls and can be configured independently of token/node budgets. Failure rolls
back the complete form; arena destruction remains iterative.

`parser_phase2_golden` compares native ASTs with pinned records and checks negative
fixtures offline. `parser_phase2_oracle` replays them with exact-version OTP 29.1.
Fixtures cover based large integers, binary64, Unicode characters/strings, multiline
strings, macros, all supported sigil prefixes, nested/empty aggregates and malformed
delimiters/tails/sigils. `parser_expressions` additionally checks retained macro
origins, group/concatenation extents, 8,193-element flat lists, rollback, and rejection
of 10,000 nested parentheses under a small explicit depth budget.

Step 5 validation (macOS arm64): fresh full C++23 configure/build and all 20 CTests
passed, including live OTP 29.1. AST/form/expression and both Phase II suites passed
ASan/UBSan; the final depth regression also passed natively. Missing/OTP 28 live
oracles explicitly skipped. Full Lizard and clang-tidy passed at unchanged thresholds
after adding an explicit active-transaction check identified by optional-access
analysis. Operators, calls, patterns and full function clauses remain pending.

## Operators and calls (step 6)

Step 5 commit: `a57d8e9`. A bounded Pratt parser consumes shared infix/prefix/call
metadata. The preprocessor now also uses the shared prefix descriptors; its grammar
and evaluator remain separate. Public operator enums distinguish unary and binary
syntax; match, catch, call and remote qualification have dedicated typed payloads.
No constant folding, callable-target checks, or binding checks occur during parsing.

Precedence follows the pinned grammar: catch 0, match/send 100, orelse 150, andalso
160, comparisons 200, list operators 300, additive/bitwise operators 400,
multiplicative operators 500, prefix operators 600, calls 750 and remote colon 800.
Nonassociative comparison/remote chains require grouping. Match/send/list and
short-circuit operators associate right; arithmetic associates left. Parentheses
retain a Group node and reset syntactic association. Separators are left for their
owning list/tuple/call/form production; expression parsing never consumes them.

Call targets and remote sides retain arbitrary expressions, including parser-valid
but semantically invalid targets. `M:F(X)` is a call on a remote expression;
`M:(F(X))` contains a call on the remote's right side. Chained calls are preserved.
Operator/call/colon tokens anchor diagnostics and nodes; their extents include the
full expression. Left-associative chains iterate over flat arena handles. Recursive
right-associative and unary chains use the same configurable nesting bound.

Step 6 fixtures cover every operator, both orders of representative precedence
pairs, catch, nested grouping, quoted operator atoms, dynamic/chained calls, remote
expressions and malformed operands/arguments/comparison/colon chains. Native tests
also inspect typed tree shapes and anchors, rejection rollback, 8,192 left-associated
operations, and bounded right/unary recursion. Test dump operator spellings are
independent of the production metadata, so mismapped enums fail OTP comparison.

Step 6 validation (macOS arm64): fresh full C++23 build and all 20 CTests passed,
including live OTP 29.1. The five preprocessing regressions passed again after
shared-prefix migration; five AST/form/expression/Phase II suites passed ASan/UBSan.
Full Lizard and clang-tidy passed without suppressions or relaxed thresholds.
Patterns, guards, and complete function clauses remain pending step 7.

## Patterns, guards and function clauses (step 7)

Step 6 commit: `cb556d6`. `Function` replaces the temporary `ZeroArgumentFunction`.
Every published function owns a nonempty ordered clause vector; each clause retains
restricted argument patterns, an optional guard, a nonempty expression body, and
its complete source extent. The form's name and first clause's arity apply to every
clause. Name/arity mismatch rejects the entire form at the offending head. Duplicate
function definitions in separate forms remain available for later semantic checks.

`PatternSyntaxId` now addresses its own transactional arena, with immutable module
access and exhaustive visiting. `RestrictedPattern` wraps expression payloads
parsed through the `pat_expr` entry point. Root calls, remote qualification, send,
short-circuit operators and catch are excluded there; aliases, prefix, arithmetic,
comparison and list operators follow OTP. Grouping preserves that grammar context.
The tuple/list productions intentionally parse general expressions inside them:
`f(g()) -> ok.` fails, while `f({g()}) -> ok.` parses and later fails lint. This is
syntax classification, not a guarantee of semantic pattern validity. Records and
binary patterns remain assigned to steps 8–9.

`PatternCandidate` explicitly wraps expressions for permissive pattern positions.
Its storage, visiting and child-ownership rules are implemented and tested now;
control-flow productions such as `case X of f() -> ok end` arrive in step 10.
Function argument construction rejects candidates, since those heads use the
restricted grammar. Both wrappers reuse expression payloads rather than duplicating
literal/container/operator structures. Pattern allocations count against the node
budget and roll back together with expressions and form origins.

`GuardSyntax` contains nonempty alternatives separated by semicolons, each with a
nonempty comma-conjoined expression sequence and its own origin extent. Missing
`when` is represented by an absent optional, not an empty guard. Guard calls, match,
catch, sends and unbound names are retained if the parser grammar accepts them;
no BIF whitelist or binding analysis is applied. Semicolons before `->` separate
guard alternatives; those after body expressions separate named function clauses.

Step 7 fixtures cover macros in heads, aliases, every restricted operator, multiple
clauses/arities, guard conjunctions/alternatives, body sequences, permissive nested
containers, duplicate definitions, head mismatches and malformed heads/guards/bodies.
A pinned accepted AST plus lint-error record explicitly checks the parsing/lint
boundary. Native tests additionally cover pattern owner moves, stale/foreign IDs,
candidate distinction, source origins, nonempty construction invariants and exact
node-budget accounting across recovery and multiple forms.

Step 7 validation (macOS arm64): freshly configured full C++23 build and all 21
CTests passed. All 21 also passed in C++26 and ASan/UBSan builds, including native
AST/rejection parity and live OTP 29.1 parser/lint comparisons. Runtime-only
configure/build passed. Full Lizard and clang-tidy passed at unchanged thresholds
without added suppressions. Phase II is complete; Phase III and subsequent grammar,
semantic analysis, parse-check CLI and code generation remain future work. These
host results do not claim Linux/Windows execution coverage.
