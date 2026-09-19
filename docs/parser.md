# Parser implementation and validation

Baseline: OTP 29.1 (`751f87b703fe5948607d08e82599ce644b772e76`).

Current native test builds discover installed OTP 29+ at CMake configure time and
fail for missing/older installations. Live suites run on that selected version;
historical exact-version/skip results below describe earlier validation runs.

Implementation follows [.agents/02-parser.md](../.agents/02-parser.md).

## Phase VI step 16 — Recovery and resource contracts

Syntax diagnostics retain a stable category, logical invocation coordinates,
physical macro/include traces and, where available, the nearest unmatched opener.
Expected terminals/categories are also available in `Diagnostic::expected`.
Raw expanded-token callers supply an explicit EOF token, including its provenance.
Delimiter recognition is shared with macro argument splitting.

Each failed form rolls back all arenas and origins. Subsequent complete forms
remain available, but failure is sticky. Resource exhaustion stops the session.
Defaults are 1,000,000 tokens/form, 4,000,000 tokens/module, 1,000,000 nodes across
all arenas, 1,000 diagnostics plus one exhaustion message, recursive depth 256
(hard ceiling 512), and 16,000,000 work units. Work accounts for input tokens,
grammar entries, node creation, normalized literal contents and map insertion/
metadata sorting. These are accounting limits, not wall-clock guarantees or a
claim of linear runtime; decoded source sizes and shared binary literal limits
also affect cost. Preprocessing has its own independently configurable limits.

The public tree printer uses an iterative work queue, caps displayed indentation,
and defaults to 4,000,000 visited objects. Its optional third argument changes
that budget; exhaustion throws `std::length_error`. Flat arena destruction
does not recurse through child IDs. Consumers should also traverse iteratively
when following long flat operator chains.

`parser_hardening` covers a 12,000-operator parse/print/normalization regression,
wide lists and qualifier groups, recursive expressions/patterns/types/blocks,
repeated errors, explicit expanded EOF and macro/include origins. It reports an
observed duration without asserting an asymptotic bound. `parser_mutations`
replays 900 fixed-seed token mutations twice, verifies deterministic diagnostics
and checked traversal, and requires the next valid form to survive. Both tests
have 60-second termination bounds and run under ASan/UBSan as well.

Step 16 validation: all 39 Debug tests passed across the full run and corrected
hardening rerun; the three ASan/UBSan hardening/mutation/printing tests passed.
Fresh full Debug configuration, formatting, Lizard and clang-tidy passed on
macOS arm64 with installed OTP 29.0.5. The 12,000-operator regression took about
0.7 seconds in Debug; this is one host observation, not a scaling guarantee.

## Phase V — Attributes, types and specifications

The parser now retains ordinary literal attributes, export/import lists, legacy
module parameters, tuple/native record declarations and documentation metadata.
Normalized literal terms have distinct `TermId` handles; defaults and documentation
`equiv` calls retain expression syntax. File references are data at the parser API.
No ordinary Erlang calls, parse transforms or documentation file reads run here.

Type syntax uses a separate `TypeId` arena and grammar entry points. Unions,
annotations, ranges and type operators remain syntax. Aggregate, remote/local,
binary and fun types preserve the distinctions made by the pinned parser.
Alias/opaque/nominal declarations and mixed typed record fields are supported;
alias resolution, type checking and semantic validity remain later-stage work.

Both new arenas participate in form transactions, node budgets, source ownership,
generation checks and exhaustive public printing. Phase V fixtures and native
attribute/type tests cover normalization, rejection, macros/includes, ownership,
rollback and limits. Specifications and callbacks preserve qualification, first
signature arity, overloads, products/results and modern or legacy subtype constraints.
Different overload arities are accepted at parse time, as in OTP; lint correspondence
and type consistency are deferred. Exceptional OTP helper-builder inputs are tested
separately and produce normal recoverable parser diagnostics in ErlangAoT.

## Phase IV step 10 — Branching and receive

Step 10 commit: `d22c6d9`.

`BlockExpression`, `CaseExpression`, `IfExpression`, and `ReceiveExpression` retain
ordered syntax, including explicit timeout expressions/bodies and after-only receives.
`BranchClause` owns a `PatternCandidate`; `IfClause` owns a mandatory guard. Guard
and function clause payloads now live in `ast/clauses.hpp` to avoid recursive header
dependencies. No matching, mailbox, or guard semantics are implemented.

The parser shares sequence and optional-guard mechanics, bounds nested expressions,
and leaves each delimiter to its owning construct. `expect` now accepts keyword
delimiters while continuing to reject lexical form dots. Constructors validate child
ownership, grammar categories, extents, and mandatory nonempty sequences. Both the
private oracle projection and public iterative tree printer cover the new variants.

Authored positive/negative fixtures are in `phase4/step10`; native tests cover macro
provenance after source destruction, candidate patterns, rollback, nesting limits,
and empty-node rejection. New oracle records use installed OTP 29.0.5, checked
against the pinned 29.1 grammar; no claim of a new exact-29.1 runtime run is made.

Step 10 validation: fresh Debug configure/build, all 30 CTests (including all live
OTP suites), clang-format, and the full Lizard/clang-tidy quality gate passed on macOS arm64.

## Phase IV step 11 — Funs, try and maybe

Step 11 commit: `e7e5f16`.

Local and remote fun references preserve typed name/arity components; anonymous
and recursive named funs reuse function clauses, constructor checks, and parser
head-name/arity checks. Reference names or arities are not resolved or evaluated.

Try nodes preserve optional `of`, catch and after parts. Catch reason roots use
`RestrictedPattern`, matching `try_clause -> pat_expr` in the pinned grammar (the
plan's informal "reason candidate" wording does not relax that restriction).
Class and stacktrace omissions remain explicit; only the test oracle projection
normalizes these to `throw` and `_`. Empty catch/after parts and try without either
are rejected. Try `of` clauses remain permissive branch candidates.

Maybe bodies retain ordered expression IDs or typed `MaybeMatch` objects containing
candidate patterns. `?=` is recognized only in that body grammar. Optional else
branches reuse candidate clauses, and existing feature-sensitive tokens/snapshots
remain authoritative. Included and macro-generated fun/maybe syntax is covered.

Step 11 fixtures cover all fun-reference/try/maybe alternatives and negative head,
stacktrace, body and conditional-match boundaries. Native tests exercise omission,
typed references, source lifetime/provenance, feature-disabled atoms, rollback,
constructor invariants, deep nesting, and public printing.

Step 11 validation: fresh full Debug build, all 31 CTests with live OTP 29.0.5,
the additional alternative-fixture replay, clang-format, and full Lizard/clang-tidy
passed on macOS arm64.

## Phase IV step 12 — Comprehensions

List, map and binary comprehensions now retain typed templates and ordered qualifier
groups. List/map templates remain vectors, including OTP 29 multi-template syntax;
binary templates retain the grammar's single `expr_max`. Aggregate parsers parse
the shared prefix once and select the comprehension at `||`, with no speculative
reparse. Restricted root patterns and map updates reject comprehension tails.

`Qualifier` has closed filter/list-generator/binary-generator/map-generator variants.
Generator arrows retain strictness, inputs remain expression IDs, and pattern roots
are explicit candidates. Map generators retain separate key/value candidates. A
`ZippedQualifier` contains at least two simple qualifiers; sequential and zipped
groups are never flattened. The grammar permits filters in zip groups, so later
semantics must reject illegal combinations. Binary generator heads must be the
binary production, including rejection of sigils that otherwise normalize to the
same Bitstring payload.

Match qualifiers remain `FilterQualifier` nodes containing `MatchExpression`; the
parser does not implement `compr_assign` binding, feature validation or lowering.
Per-form/final feature snapshots survive in both enabled and disabled fixtures.
The lint oracle now forwards epp's retained feature metadata to erl_lint, matching
compile.erl: the enabled assignment fixture passes lint and the disabled fixture
fails, while both parse successfully. Historical syntax/golden records are unchanged.

Step 12 fixtures cover all three output kinds crossed with all six generator arrows,
multiple templates, exact/association map fields, mixed zipped/sequential groups,
filter-only groups, arbitrary candidate expressions, nesting, and malformed prefix/
delimiter/operator cases. Native tests check candidate categories, strictness,
source origins, feature states, flat qualifier iteration, nested budgets, rollback,
constructor invariants and public tree printing. LLVM, binding, guard legality,
record declarations/types and comprehension execution remain separate work.

Step 12 validation: fresh full Debug configure/build and all 32 CTests passed with
Homebrew OTP 29.0.5. All five parser oracle suites passed again after fixing the
lint feature handoff. Lizard, clang-tidy and formatting passed. A separate ASan/UBSan
build passed the control, exception, comprehension and iterative AST-printing tests;
the CLI printed all three new syntax families successfully. Host evidence remains
macOS arm64; no other host/toolchain validation is claimed for this phase.

The CLI now exposes the implemented grammar through `--print-ast`. It prints a compact
indented tree of recovered forms, reports diagnostics to stderr, and fails on syntax
errors. `--print-pp --print-ast` shares one preprocessing pass. See README for examples;
the dedicated diagnostics-only parse-check mode remains planned.

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

## Maps and OTP 29 records (step 8)

Step 7 commit: `3cec991`. Map construction/update nodes retain an optional base and
ordered key/value fields tagged association (`=>`) or exact (`:=`). Both operators
remain parser-valid in pattern positions; key and association legality belongs to
later semantic checks. Map keys and values use the general expression grammar.

Record nodes distinguish construction/update, field access, and index syntax.
Identities explicitly retain unresolved local names, qualified native module/name
pairs, or inferred `#_` syntax. Uppercase/variable-looking and reserved-word names
are converted only in `record_name` positions; scanner classification is unchanged.
A spaced `# _{}` is a local name `_`, distinct from inferred `#_{}`. Unqualified
names are not classified as tuple/native records until declaration analysis.
Fields preserve source order, atom/variable names (including `_`), explicit values,
and origins. Omitted fields remain omitted; no layout/default expansion occurs.

Structural postfix parsing follows the pinned productions rather than applying a
universal postfix rule. Maps chain on primary/map bases; local records chain on
primary/record bases. Qualified/inferred record postfixes require primary bases.
Calls and mixed map/record chains need parentheses where OTP requires them. Pattern
roots permit creation/index syntax but exclude updates/accesses; nested container
expressions remain permissive. Index names must be unqualified atoms. Lexical
record dots remain distinct from form-ending dots.

The existing private projection/harness is reused for `parser_phase3_golden` and
`parser_phase3_oracle`, preserving both field operators, record identities, ordering,
and bases. Step 8 fixtures cover every reserved record name, contextual variables,
macros, mixed operator boundaries, chaining, malformed fields/indexes/modules, and
parser-accepted/lint-rejected syntax. Native tests additionally check source spans,
macro ownership, foreign child/source rejection, rollback, iterative 4,096-update
chains, and nesting limits. New nested field/identity extents are builder-validated.

Step 8 validation (macOS arm64): fresh full C++23 build and all 24 CTests passed,
including native/offline/live OTP 29.1 AST and rejection comparisons. Seven relevant
parser suites passed ASan/UBSan. Full Lizard and clang-tidy passed with unchanged
thresholds and no added suppressions. Binary syntax remains pending step 9.

## Binary and bitstring syntax (step 9)

Step 8 commit: `d17cda4`. `Bitstring` stores ordered `BinarySegment` values with
optional explicit sizes and optional nonempty modifier lists. Modifiers retain an
atom name, optional arbitrary-precision integer parameter, and source extent.
Omitted size/types are distinct from explicit `default` atoms. Unknown/duplicate
modifiers, incompatible combinations and out-of-range units remain syntax for later
validation. No segments are evaluated and no runtime byte layout is imposed.

Segment values use `bit_expr`: `expr_max`, optionally preceded by one unary
operator. Sizes use `expr_max` without a bare prefix. These productions exclude
bare calls, maps, records and infix expressions; grouping re-enters the general
expression grammar. Thus `<<A:B>>` means value A with size B, and `<<(A/B):(N/2)>>`
contains grouped arithmetic. Unparenthesized arithmetic, calls, repeated prefixes
and negative sizes are rejected where the pinned grammar rejects them. Pattern
positions use these same productions without imposing semantic pattern checks.

The temporary `BinarySigilLiteral` payload is replaced by ordinary Bitstring nodes.
Binary sigils construct a decoded StringLiteral segment with omitted size and one
`utf8` modifier, matching `erl_parse:build_sigil`. The child string retains its own
token extent, the implicit modifier points to its prefix origin, and the segment
covers the sigil with the string as anchor. No escapes are decoded twice. The
private projection retains its historical `binary_sigil` label for this exact
abstract shape, including equivalent explicit `<<"text"/utf8>>` syntax, so earlier
pinned records remain unchanged. Other binaries expose every segment/default/type
in the projection. String sigils continue to produce StringLiteral nodes.

Native tests cover explicit/omitted defaults, large modifier integers, source
extents, macro sigil provenance, binary patterns, parser-versus-lint behavior,
transaction rollback, nested-binary exhaustion under default/custom limits, and
4,097-element segment/modifier spines. Builder checks include foreign sizes and
modifier sources and rejection of empty explicit type lists. Restricted binary
primaries share the ordinary expression nesting guard, closing a separate recursive
path. The existing preprocessor bitstring evaluator remains unchanged.

Step 9 validation (macOS arm64): freshly configured full C++23 build and all 25
CTests passed. All 25 also passed in C++26 and ASan/UBSan builds, including live
OTP 29.1 comparisons. Runtime-only configure/build passed. Missing and OTP 28
oracles explicitly skipped live Phase III replay while offline/native tests still
ran. Full Lizard and clang-tidy passed with unchanged thresholds and no added
suppressions. Phase III is complete; control flow, comprehensions, remaining
attributes/types, semantic validation and the parse-check CLI remain later steps.
Linux/Windows execution coverage is still outstanding.
