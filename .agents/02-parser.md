# Erlang syntax parser and typed AST implementation plan

Status: steps 1–8 implemented, 2026-09-19; steps 9–18 remain pending.
See [validation and current grammar limits](../docs/parser.md). Full Erlang parsing
and the parse-check CLI are not yet implemented. Host evidence is macOS arm64.
Prerequisite: [01-pp.md](01-pp.md), steps 1–13, is implemented; retain its tests
and [documented compatibility policies](../docs/preprocessor.md).

## Objective and boundaries

Consume native preprocessing output and produce an owned, clean, type-safe C++
syntax AST for Erlang/OTP 29.1. The normal pipeline is:

```text
SourceManager → existing Lexer ↔ PreprocessorSession
             → expanded OrdinaryForm tokens + diagnostics + feature context
             → form parser → typed module syntax AST
             → future semantic analysis → future lowering/code generation
```

“Preprocessed source” means the existing in-memory expanded tokens. Do not print
and re-lex them: that loses token identity, feature-sensitive classification, and
macro/include provenance. If tests need source-text input, use the existing Lexer
or PreprocessorSession. Do not introduce another Erlang lexer.

This plan covers ordinary source forms, expressions, pattern syntax, guards,
attributes, records, type declarations, and specifications. It includes syntax
introduced in OTP 29, even where execution will require later runtime work.
The AST is syntax, not a type-checked or bound program. Name resolution, variable
binding, legal guard BIF checks, record resolution/expansion, type checking, parse
transform execution, documentation-file ingestion, lowering, LLVM, and runtime
implementation remain separate work. Retain the declarations those passes need.

Only reserve `compiler/src/stage_readers/preprocessed/`, `abstract/`, and `ir/`.
Do not select a public interchange format, implement stage readers, or turn a
test AST dump into a public serialization contract.

## Reference baseline and compatibility rules

Use the same pinned checkout as preprocessing: `references/otp/`, tag `OTP-29.1`,
commit `751f87b703fe5948607d08e82599ce644b772e76`. It is an ignored research
checkout and optional oracle, never a normal build dependency.

Primary implementation references:

- [Pinned erl_parse.yrl](https://github.com/erlang/otp/blob/751f87b703fe5948607d08e82599ce644b772e76/lib/stdlib/src/erl_parse.yrl):
  productions, precedence, `parse_form/1` token preparation, and `build_*` actions.
- Local `lib/stdlib/src/erl_parse.erl`: generated executable parser, inspected
  alongside its `.yrl` source. It exists in the locally built OTP checkout but
  can be absent from a fresh source checkout; do not require or hand-port its
  generated Yecc tables. The project path is `references/otp/lib/stdlib/`.
- Local `lib/stdlib/src/erl_scan.erl`, `epp.erl`, `erl_lint.erl`, and
  `lib/stdlib/src/erl_features.erl`: lexical boundaries, stage responsibilities,
  feature handling, and parser-versus-linter acceptance.
- Local `lib/compiler/src/compile.erl` and `v3_core.erl`, together with
  `lib/stdlib/src/erl_expand_records.erl`: consumer requirements and the boundary
  between source AST, validation, record expansion, and lowering.
- Local `system/doc/reference_manual/{expressions,patterns,typespec,
  ref_man_records,ref_man_native_records}.md` and the abstract-format documents.
- [erl_parse API](https://www.erlang.org/doc/apps/stdlib/erl_parse.html),
  [abstract format](https://www.erlang.org/doc/apps/erts/absform.html), and
  [expressions](https://www.erlang.org/doc/system/expressions.html) for navigation.
  Live pages can describe different patch releases; the pinned source wins.

Compare parsing with `erl_parse:parse_form/1`, and complete preprocessing/parsing
with `epp:parse_erl_form/1`. Do not use whole-program compilation as the syntax
oracle: it also rejects programs for semantic reasons. Attribute builders and
clause-name/arity checks performed inside `erl_parse` are part of this parser's
compatibility target. Separately characterize linter-only restrictions.

In particular, `erl_parse` uses a restricted `pat_expr` in function/fun heads,
but intentionally accepts general expressions in some other pattern positions.
Guard grammar also accepts expressions before guard legality is checked. Preserve
these distinctions instead of quietly enforcing a stricter language in the AST
constructor. Feature `compr_assign` is checked by `erl_lint`; recognizing its
syntax must not falsely claim that the feature is enabled.

OTP's `%ssa%` compiler-test annotations are outside ordinary Erlang source scope.
Record this explicit exclusion in the coverage matrix; select comprehension
fixtures by their contents rather than similarly named unrelated test suites.

### Findings from the existing OTP parser/compiler

| Inspected reference | Consequence for this plan |
| --- | --- |
| `erl_parse.erl` / `erl_parse.yrl`: `parse_form/1` | `spec`, `callback`, and `record` receive contextual treatment only at the attribute prefix; they are not new globally reserved lexer words. |
| `build_function`, `build_fun`, `build_attribute`, `build_typed_attribute`, `build_sigil` | Parsing includes builder validation and normalization, not just recognition of grammar productions. Include `-nominal` alongside `-type` and `-opaque`. |
| `compile.erl`: `do_parse_module/2` | OTP initializes scanner feature state, asks epp for parsed forms and extra metadata, and forwards the final enabled-feature set to later passes. Preserve both the module's final context and any per-form snapshots used internally. |
| `compile.erl`: `standard_passes/0`, `abstr_passes/1` | Parse transforms precede lint; record expansion and Core conversion occur later. Keep parsing independent of all four operations. |
| `erl_expand_records.erl`: module setup and record handlers | Record declaration/import information is needed downstream. Preserve unresolved source record identities and defaults; do not encode tuple/native runtime layouts in the parser. |
| `v3_core.erl`: `maybe_match_exprs`, `preprocess_quals`, `rewrite_compr_assign` | `maybe`, strict/zipped generators, and comprehension assignments are lowered after parsing. Preserve their source structure instead of copying Core transformations into AST construction. |

This is a behavioral reference and selective test reuse, not an Erlang-to-C++ port
of the entire OTP compiler or its generated parser machinery.

## Reuse decisions

| Existing component | Decision for this plan |
| --- | --- |
| `source.hpp`, `source/source.cpp` | Reuse owned buffers, decoding, physical coordinates, source lifetime, and UTF conversion. |
| `lexer.hpp`, `lexer/{lexer,numbers,literals}.cpp` | Reuse tokens, arbitrary-size decimal integers, literal decoding, incremental forms, feature keywords, and dot classification. Extend only proven lexical gaps. |
| `diagnostic.hpp`, `diagnostics/diagnostic.cpp` | Reuse severity, rendering, logical positions, and related spans; add parser diagnostic codes. |
| `PreprocessorSession`, `OrdinaryForm` | Consume expanded forms and diagnostics directly; add a small feature-context handoff, not another preprocessing driver. |
| `preprocessor/token_utils.{hpp,cpp}` | Move syntax matching and generic token-to-diagnostic construction into compiler-private shared helpers when both parsers need them. Leave generated tokens/stringification with preprocessing unless a real second use exists. |
| `preprocessor/cursor.hpp` | Extract bounded cursor mechanics; retain directive-specific names, messages, and category rules in a small directive adapter. |
| `preprocessor/expression_parse.cpp` | Share operator metadata and trivial token operations after tests. Its restricted grammar and positional `Expr.children` model are not the full-language AST. |
| `preprocessor/{value,terms,expression,operators,guards,bits}.*` | Keep conditional evaluation and guard catalogs private. Extract literal-only normalization only where attribute parsing proves identical behavior. Never run ordinary source expressions through the condition evaluator. |
| `parsing/{boost_parser.hpp,probe.cpp}` | Keep the pinned character-parser boundary and token-iterator rejection probe. Use explicit token parsing; do not repeat the failed Boost token-adapter experiment. |
| CMake, quality tooling, native tests, escript/oracle adapters | Extend the existing targets and exact-version/offline testing approach. Runtime-only builds remain independent of frontend dependencies. |

Avoid a generalized parser framework or wholesale preprocessor rewrite. Share
small mechanisms with demonstrated common behavior; grammar-specific decisions
stay in their grammar. Any intentional preprocessor bug fix discovered during
extraction needs an oracle-backed regression and a documented behavior change.

## Intended AST contract

- Use a compiler-owned `ast` namespace and closed `std::variant` node families
  with named payload structs. Fields describe their meaning: `callee`, `arguments`,
  `left`, `right`, `clauses`, `timeout`, `size`, and so on. No string node tags,
  `std::any`, unchecked casts, or universal `kind + children` bags.
- Prefer module-owned arenas with distinct `ExprId`, `PatternSyntaxId`, `TypeId`,
  and `FormId` handles. Construct handles through the owning builder and access
  them through category-checked APIs. IDs are local to one AST owner; no cross-owner
  references. Vector growth must not invalidate handles, and callers must not
  retain references across builder mutations. Ownership is move-only initially.
- Use distinct atom/variable/operator types and payloads, reusing `Integer` and
  decoded Unicode values. No host-width narrowing of Erlang integer literals.
  Keep Boost and runtime term representations out of public AST headers.
- Model optional syntax with `std::optional`, grammar alternatives with variants,
  and required/nonempty sequences with checked construction. Examples include
  proper versus improper list tails, omitted binary size versus explicit size,
  and a receive expression with versus without an `after` part.
- Distinguish restricted pattern syntax from an explicit `PatternCandidate`
  wrapper around expression syntax in OTP's permissive positions. This wrapper
  means “requires later pattern validation,” not “validated pattern.” Likewise,
  `GuardSyntax` holds nonempty alternatives of nonempty expression sequences;
  it does not assert that every call is a legal guard BIF. Shared structural
  payload templates may reduce expression/pattern duplication where useful.
- Use typed clause families for function/fun heads, case/receive branches,
  guard-only `if` branches, and exception handlers. Keep type expressions in
  their own family, including source type variables and unresolved type names.
- Preserve source order, operator identity, guards' comma/semicolon grouping,
  omitted defaults, and syntax needed by later passes. Do not desugar records,
  comprehensions, matches, or control flow into backend constructs.
- Give every node source metadata: logical anchor, expanded-token extent, and
  physical spelling/expansion origins. A macro-expanded node may span several
  files; never invent one contiguous physical span between unrelated buffers.
  Use an owned per-form origin table plus ranges to avoid copying entire traces
  into every ancestor. Retain source owners after tokens/sessions are destroyed.
- Completed ASTs are read-only to consumers. A failed form commits no ordinary
  AST node. A module parse result may contain valid forms plus separate failed-form
  records/diagnostics, but its success flag remains false. Later compilation must
  require a successful result before accepting the AST.

Freeze the concrete ownership/source API in step 3 with working code and tests.
This contract is an implementation direction, not an ABI or stage-file format.

## File placement

Create files as each step needs them; do not add empty speculative modules.

| Location | Responsibility |
| --- | --- |
| `compiler/include/erlang_aot/compiler/ast/{ids,source,expressions,patterns,types,forms,module}.hpp` | Typed syntax, owner-bound handles, provenance, and read-only module access. |
| `compiler/include/erlang_aot/compiler/parser.hpp` | Form/module parse options, limits, results, and entry points. |
| `compiler/src/ast/` | Builders, invariant checks, source tables, and traversal. |
| `compiler/src/parsing/{token_cursor,token_syntax,operator_info}.*` | Small shared token mechanics; existing Boost probe remains here. |
| `compiler/src/parser/{parser,forms,expressions,patterns,clauses,control,comprehensions,records,binaries,attributes,types}.*` | Split grammar implementation with small focused functions. |
| `compiler/src/driver/` | Pipeline/CLI orchestration extracted from `main.cpp` when needed. |
| `tests/compiler/parser/`, `tests/fixtures/parser/` | Native AST tests, private dump/oracle helpers, sources, and pinned records. |
| `docs/parser.md` | Implemented coverage, AST contract, limits, known differences, validation evidence. |

## Required completion and commit protocol

The numbered steps below are sequential and independently buildable. Each step
has substeps, a success condition, and a required commit. **Complete and commit
each step before starting the next one; do not batch several steps into one
commit.** Suggested commit subjects follow each step. If a step needs subdivision,
document it first and apply this same protocol to every resulting step.

For every step:

1. Implement its scope and positive, negative, and interaction fixtures. Keep
   public APIs usable and gate incomplete CLI functionality explicitly.
2. Add 1–2 line intent comments for new functions and fields. Keep functions/files
   small; run the existing `make format` target or `clang-format` on C++ changes.
3. Freshly configure `build/debug` with compiler, runtime, and tests enabled using
   `cmake --preset debug` (preserve documented local dependency/oracle overrides).
   Build and run the relevant tests plus the existing preprocessor/CLI regressions.
4. Run `cmake --build build/debug --target check-quality`. Both Lizard and
   clang-tidy must pass. Do not raise thresholds or add suppressions to pass.
5. Record evidence and limitations in `docs/parser.md`; update this plan's status,
   `.agents/arch.md`, `.agents/files.md`, and `aimemory.md` as appropriate. Review
   `git diff --check`, scope the diff, and exclude unrelated edits/build artifacts.
6. Create the required commit. Record its hash with the step's evidence in the
   subsequent progress record. A failed gate leaves the step incomplete.

These requirements concern future implementation commits; writing this plan does
not mean that implementation steps have been performed.

## Phase I — Baseline, reuse, and AST foundation

### Step 1. Inventory the pinned grammar and establish the parser oracle

1. Map every ordinary `erl_parse.yrl` production and relevant `build_*` action to
   a coverage row, AST family, owning step, and fixture. Include contextual
   `spec`/`callback`/`record` handling and parser-time attribute normalization.
   Cross-check generated `erl_parse.erl` and `compile.erl` consumers against the
   reference findings above; use `v3_core.erl` only to identify deferred lowering.
2. Seed cases from `erl_scan_SUITE`, `erl_lint_SUITE`, compiler `lc_SUITE`,
   `mc_SUITE`, `bs_bincomp_SUITE`, `bs_match_SUITE`, `bs_construct_SUITE`,
   `fun_SUITE`, `maybe_SUITE`, `record_SUITE`, and `native_record_SUITE`.
   Inspect actual cases; preserve licenses for copied fixtures. Classify syntax
   errors separately from lint errors and runtime expectations.
3. Extend the existing version-checked escript infrastructure for raw form-token
   parsing and end-to-end epp parsing. Add successful, parser-rejected, and
   parser-accepted/linter-rejected examples; record exact OTP build provenance.
4. Define a private structural comparison format. Normalize annotations and
   fixture-root paths separately from structure; preserve integer precision,
   float bits, clause order, operators, and native-record identities. Native
   provenance assertions remain separate from OTP's simpler annotations.

Success: oracle smoke cases and offline golden records run in CTest; absent or
wrong-version OTP explicitly skips live comparisons, without skipping native or
offline tests. Every in-scope grammar family has an assigned implementation step.

Required commit: `test(parser): pin OTP grammar coverage and oracle`.

### Step 2. Extract shared token mechanics and audit lexical completeness

1. Extract a bounded, category-aware cursor with lookahead, consume, checkpoint,
   restore, offset, and explicit EOF anchor. Keep diagnostic creation injectable
   or at the grammar boundary; no hard-coded `malformed_directive` in the core.
2. Move reusable syntax matching and operator descriptors to `src/parsing/`.
   Describe precedence, associativity, and applicable grammar contexts explicitly;
   preserve the preprocessor's restricted operator set through an adapter.
3. Migrate directive/condition consumers only where mechanics are identical.
   Keep macro argument balancing and function-context recognition in preprocessing:
   they operate on partial syntax and must not depend on the full parser.
4. Compare existing token kinds and punctuation with all pinned grammar terminals:
   `#_`, `?=`, `&&`, `<:-`, `<:=`, `::`, `..`, `...`, sigil triples, and lexical
   dots. Add scanner fixes only for demonstrated gaps, including quoted-keyword
   and longest-match regressions. Contextual attribute names remain atoms.

Success: shared helpers serve both stages; existing preprocessor/scanner records
remain unchanged except separately evidenced fixes. Tests distinguish an atom
`'end'` from keyword `end` and symbol `.` from a form-ending `TokenKind::dot`.

Required commit: `refactor(frontend): share bounded token parsing primitives`.

### Step 3. Implement AST ownership, node construction, and source metadata

1. Implement the owner/arena and typed handle design above with initial literal,
   variable, and form payloads. Add node families incrementally in later steps.
   Prevent default invalid handles from masquerading as valid children.
2. Implement source extents over owned form-origin tables, including generated
   tokens and noncontiguous macro/include origins. Define anchors for empty and
   synthetic constructs without accessing an absent token/source.
3. Add transactional builder checkpoints so a failed form removes its allocations
   and origin entries without disturbing earlier committed forms. Bound rollback,
   traversal, and destruction work; avoid recursive destruction of long chains.
4. Add exhaustive visiting and a private test dump. Document invariants and keep
   public headers free of Boost, preprocessor evaluator values, and runtime types.

Success: compile-time checks reject mixing handle categories; native tests cover
arena growth, owner moves, rollback, traversal, and AST use after source manager,
token vectors, and preprocessing session destruction. Sanitizers report no errors.

Required commit: `feat(ast): add typed ownership and source provenance`.

### Step 4. Connect preprocessing to a transactional form parser

1. Add a form entry point over expanded tokens and a module driver consuming
   `PreprocessorSession::next()`. The parser handles diagnostics and ordinary forms
   explicitly; an unexpected directive event is a contract error, not a dropped form.
2. Expose a small immutable feature-context snapshot from the session and associate
   it with each emitted form; retain the final module feature set at EOF as OTP's
   compiler does. Cover CLI feature settings, source directives,
   and includes; do not reconstruct state from already removed directives or rerun
   preprocessing. Preserve the scanner's existing token classification.
3. Add parser limits and structured syntax diagnostics. Require full consumption
   through exactly one form terminator; diagnose trailing tokens/missing dots.
   The driver may continue at the next supplied form after a failed transaction.
4. Initially support a minimal `-module(name).` and zero-argument literal-returning
   function to prove the full pipeline. Mark other grammar families as pending;
   never discard tokens or manufacture successful placeholder nodes.

Success: source containing macros/includes yields correctly located minimal AST
forms; preprocessing and parser errors latch failure, warnings do not, and a valid
form following an invalid form survives. Interleaved modules share no state.

Required commit: `feat(parser): consume expanded forms with feature context`.

## Phase II — Expressions, patterns, and function clauses

### Step 5. Parse literals and basic aggregate expressions

1. Construct atom, variable, integer, float, character, string, tuple, and list
   nodes from decoded token values. Preserve wildcard spelling without binding it.
2. Parse grouping and proper/improper list tails; preserve tuple/list distinctions
   and reject empty elements, trailing commas, and extra tails as the oracle does.
3. Concatenate adjacent string tokens at the parser layer. Implement pinned sigil
   prefix/suffix validation and string/binary meaning from `build_sigil`, using
   already decoded contents rather than decoding escapes again.
4. Add literal-origin tests for macros, Unicode, multiline strings, based numbers,
   large integers, and character versus integer syntax. Sigil output may use a
   dedicated typed literal payload until general binary syntax lands in step 9.

Success: literal/aggregate AST structure and rejected inputs agree with the oracle;
numeric values retain precision and source metadata survives string concatenation.

Required commit: `feat(parser): parse literals and aggregate expressions`.

### Step 6. Implement operator precedence, calls, and remote syntax

1. Build a precedence-climbing or Pratt expression parser using shared descriptors.
   Implement prefix arithmetic/boolean operators, arithmetic, bitwise, comparison,
   list, short-circuit, match, send, and low-precedence unary `catch` syntax.
2. Match pinned precedence/associativity, including nonassociative comparisons,
   right-associative match/send/list operators, and parentheses that reset grouping.
   Do not derive the full grammar solely from the preprocessor's subset table.
3. Parse local/dynamic calls, chained calls where accepted, and remote qualification.
   Retain general remote expression syntax where `erl_parse` accepts it; defer
   callable-target validity. Use typed match/call payloads and operator enums.
4. Test ambiguous boundaries: `a + b * c`, `A = B = C`, `A ! B ! C`,
   `catch f() + 1`, parenthesized comparisons, `M:F(X)`, and quoted operator atoms.
   Make delimiter stopping context explicit rather than swallowing separators.

Success: fixture AST shapes match OTP for every operator and representative
cross-precedence pair; malformed or chained nonassociative syntax fails locally.

Required commit: `feat(parser): add Erlang expression precedence and calls`.

### Step 7. Add pattern syntax, guards, and complete function forms

1. Add a restricted pattern entry point matching the pinned `pat_expr` productions,
   including aliases and operator syntax OTP parses before later constant/pattern
   checks. Respect its permissive nested container productions; do not recursively
   assume all children have already passed semantic pattern validation.
2. Introduce explicit expression-based pattern candidates for permissive positions.
   For example, record that `case X of f() -> ok end` can pass parsing while later
   failing lint. AST type safety must not silently change parser acceptance.
3. Implement argument patterns, optional `when`, comma-conjoined guard tests,
   semicolon-separated guard alternatives, nonempty expression bodies, and
   semicolon-separated function clauses terminated by a lexical dot.
4. Enforce the parser's clause-name and arity consistency checks. Leave duplicate
   definitions across forms, unbound variables, and legal guard calls to later
   semantics. Extend pattern coverage alongside records and binaries in steps 8–9.

Success: multi-clause functions, aliases, guard grouping, empty/nonempty arities,
head mismatch, and parser-versus-linter fixtures match the pinned parser. Every
published function form is structurally complete.

Required commit: `feat(parser): add patterns guards and function clauses`.

## Phase III — Maps, records, and bit syntax

### Step 8. Implement maps and record expression/pattern syntax

1. Parse map creation/update, association/exact fields, and map patterns with named
   key/value fields. Retain both field operators where the grammar accepts them;
   map-key legality and pattern association restrictions belong to later checks.
2. Parse tuple-record construction, update, field access, and index expressions.
   Preserve source field order, explicit defaults, and wildcard field syntax;
   do not look up layouts or expand records.
3. Cover native record identities: local names, explicit module qualification,
   inferred `#_`, and variable-looking/reserved-word names accepted by `record_name`.
   Convert names contextually without changing global lexer classification.
4. Follow the grammar's precise postfix/chaining restrictions. Unqualified record
   uses can be unresolved between tuple/native records until declarations/imports
   are analyzed; model that fact rather than guessing by spelling.

Success: map/record ASTs match pinned records, including `#State{}`, `#mod:rec{}`,
`#_{}`, updates/accesses, and record-dot versus form-dot cases. Undefined record
names still parse when OTP leaves their validation to later stages.

Required commit: `feat(parser): add maps and OTP 29 record syntax`.

### Step 9. Implement binary and bitstring syntax

1. Add typed binary segments with value, omitted/explicit size, and an ordered
   list of type modifiers. Represent atom modifiers and atom/integer modifiers
   structurally, retaining locations and explicit versus omitted defaults.
2. Match the distinct `bit_expr` and `bit_size_expr` grammar restrictions; test
   parentheses around arithmetic, unary segment values, strings, and nested bits.
3. Support binary construction and pattern positions without evaluating segments.
   Keep syntactically accepted unknown/duplicate modifiers for later validation
   when OTP does; do not replace the existing preprocessing evaluator.
4. Connect binary sigil literals to the established representation or a documented
   source-literal alternative with equivalent test projection. Preserve UTF-8
   meaning without imposing runtime byte layout on general binary expressions.

Success: bit-syntax structure, default markers, modifier order, and syntax errors
match OTP; expression `/` and `:` never consume segment boundaries incorrectly.

Required commit: `feat(parser): add binary construction and pattern syntax`.

## Phase IV — Control flow, funs, and comprehensions

### Step 10. Parse blocks, branching, and receive

1. Implement `begin`, `case`, and guard-only `if` with distinct clause structures.
   Share delimited body and branch mechanics with function parsing where identical.
2. Implement receive clauses, clauses plus `after`, and `after`-only receive.
   Represent timeout expression and nonempty timeout body explicitly.
3. Define context-specific terminators for nested bodies/guards; test commas and
   semicolons in nested blocks, missing `of`/`end`, and illegal empty bodies.
4. Test the full preprocessing path with macro arguments containing each block,
   retaining existing macro argument-balancing behavior and provenance.

Success: nested control forms match the oracle; delimiters stay with their owning
construct, and failed blocks do not consume a subsequent preprocessed form.

Required commit: `feat(parser): add branching blocks and receive expressions`.

### Step 11. Parse funs, exceptions, and maybe expressions

1. Implement local/remote fun references, permitted atom/variable components,
   anonymous fun clauses, and recursive named fun clauses. Reuse parser-level
   clause consistency checks; retain named versus anonymous distinctions.
2. Implement `try` with optional `of`, catch clauses, and/or `after` according to
   the grammar. Represent exception class, reason candidate, and optional
   stacktrace variable; document the oracle projection of omitted defaults.
3. Parse `maybe`, body expressions and conditional `?=` matches, and optional
   `else` branches. Keep `?=` in its permitted context; reuse keyword state already
   encoded in tokens, including feature-disabled words used as atoms.
4. Cover fun references versus `end`-terminated funs, nested `try`/unary `catch`,
   mixed catch-clause forms, illegal stacktrace syntax, and missing mandatory parts.

Success: every fun/try/maybe grammar alternative and negative boundary case has a
matching oracle fixture; feature-sensitive source is interpreted consistently
through macros/includes and the parser.

Required commit: `feat(parser): add fun try and maybe expressions`.

### Step 12. Parse all comprehension forms and qualifiers

1. Implement list, binary, and map comprehensions without duplicating aggregate
   parsing. Delay committing ambiguous prefixes until `||` or aggregate delimiters
   determine the construct; avoid unbounded speculative reparsing.
2. Model list/map generators (`<-`, `<:-`), binary generators (`<=`, `<:=`), filters,
   and zipped qualifier groups (`&&`) as typed alternatives with ordered children.
   Preserve permissive qualifier grammar and defer generator-pattern legality.
3. Include OTP 29 multiple list/map template forms, mixed generators, strictness,
   nested comprehensions, and map key/value patterns. Do not assume a comprehension
   always has exactly one template expression.
4. Preserve match qualifiers used by `compr_assign` and their feature context.
   Match parser acceptance with the feature both enabled and disabled; document
   the later feature/legality check instead of silently treating parsing as approval.

Success: every generator/operator/template combination in the coverage matrix has
positive and negative fixtures, and AST shape agrees with the oracle for zip versus
sequential qualifiers. Feature context reaches the AST without reprocessing.

Required commit: `feat(parser): add OTP 29 comprehensions and qualifiers`.

## Phase V — Attributes and type syntax

### Step 13. Parse ordinary attributes and record declarations

1. Replace the minimal module parser with full attribute dispatch, including
   contextual `record`, `spec`, and `callback` recognition at form start only.
   Cover parenthesized and unparenthesized forms accepted by the pinned parser.
2. Add typed module, export/import, export-type, file, behavior/behaviour, compile,
   on-load, optional-callback, export-record, and import-record payloads where
   useful. Keep arbitrary attributes through a typed literal-term value family,
   including nested `Name/Arity` notation where OTP normalizes it.
3. Implement untyped tuple/native record declarations and field defaults using
   expression syntax. Keep attributes in source order, including implicit `-file`
   events; their AST representation must not apply logical file remapping twice.
4. Implement `doc`/`moduledoc` parser-level value forms, including special `equiv`
   expressions and metadata. Preserve file references without opening them. Retain
   parse-transform options as data without attempting to execute transforms.
5. Share only proven literal normalization with preprocessing. Match `build_attribute`
   rejection/normalization, including legacy module forms accepted by the parser;
   do not accept arbitrary expression operands as generic attributes by accident.

Success: known/unknown attributes, malformed shapes, nested arity notation, record
defaults, native record imports/exports, and documentation values match the oracle.
No attribute parsing executes ordinary Erlang calls or introduces runtime coupling.

Required commit: `feat(parser): add attributes and record declarations`.

### Step 14. Add type syntax and typed declarations

1. Implement separate type precedence and nodes for variables, singleton literals,
   annotations, unions, ranges, parentheses, and permitted operator expressions.
   Do not reuse expression precedence unchanged or evaluate types through guards.
2. Parse local/predefined and remote type applications; tuples; empty, list, and
   nonempty-list types; map association/exact types; record field refinements;
   binary base/unit types; and fun types with fixed or arbitrary argument lists.
3. Add `-type`, `-opaque`, and `-nominal` declarations and typed tuple/native record fields,
   including fields with defaults and mixed typed/untyped declarations. Audit
   `build_typed_attribute` for any further baseline forms or special checks.
4. Preserve declared type parameters and source names. Apply only distinctions
   made by the pinned parser; defer alias resolution, type arity/name validity
   beyond parser checks, recursive-type rules, and type checking.

Success: the type grammar coverage matrix is complete; representative nested
types and rejected separators/annotations match OTP. Expression nodes cannot be
passed to type-child APIs by mistake.

Required commit: `feat(parser): add Erlang type syntax and declarations`.

### Step 15. Add specifications, callbacks, and constraints

1. Parse local and qualified specification names and every accepted envelope,
   overloaded signatures, callback forms, result types, and `when` constraints.
2. Represent argument products, result type, and constraint alternatives with
   named typed payloads. Include legacy constraint forms accepted by the pinned
   `build_compat_constraint` action rather than assuming only `Var :: Type`.
3. Enforce specification structure and builder checks performed by `erl_parse`;
   leave correspondence with actual functions, callback implementations, and
   semantic type consistency to later passes.
4. Close every attribute/type coverage row, including unusual `record` helper
   paths in `parse_form/1`; add minimized fixtures for behavior that differs from
   the reference-manual presentation.

Success: overloaded/qualified specs, callbacks, bounded fun types, and malformed
constraints match the oracle; all ordinary form families have a typed AST mapping.

Required commit: `feat(parser): add specifications callbacks and constraints`.

## Phase VI — Diagnostics, integration, and compatibility completion

### Step 16. Harden diagnostics, recovery, and resource limits

1. Complete stable parser diagnostic categories, expected-token messages, and
   construct/opener locations. Use logical invocation locations and related
   physical origins for macro/include failures, including EOF after expansion.
2. Keep recovery transactional at preprocessor-provided form boundaries. Add
   local delimiter recovery only where evidence warrants it; no partially valid
   nodes or invented semantic children. Every recovery path advances or ends.
3. Enforce configurable depth, node, token/work, and diagnostic budgets. Cover
   long flat lists, right-associative chains, nested patterns/types/blocks, large
   qualifier lists, and repeated failures. Include AST visiting/dumping and
   teardown in stack-safety checks; parser recursion limits alone are insufficient.
4. Add bounded generated/mutated token tests and optional fuzz targets with fixed
   regression seeds. Assert termination, no invalid handles, deterministic error
   order, and useful subsequent valid forms; classify limits separately from
   language errors. Do not claim linear behavior without measuring stress cases.

Success: hostile/malformed fixtures terminate within documented budgets, sanitizer
runs are clean, and multiple recovered forms cannot clear the module failure flag.

Required commit: `fix(parser): harden recovery provenance and resource limits`.

### Step 17. Integrate a parse-check CLI and document the AST API

1. Add `erlangaot --parse-check` using existing preprocessing options and the
   module parser. Specify interaction with `--preprocess-check`, `-o`, multiple
   inputs, and `--`; retain usage-error versus source-error exit conventions.
2. Share source loading/options/diagnostic orchestration with preprocessing mode;
   extract focused driver helpers instead of expanding `main.cpp` complexity.
   Each input uses an isolated source/preprocessing/parser/AST ownership context.
3. Report syntax success honestly: parse-check does not promise semantic validity,
   successful parse transforms, or native executable generation. Do not create or
   overwrite output files in check modes. Keep AST dumps private to tests initially.
4. Document examples, AST ownership/visiting/source APIs, feature-context handoff,
   supported grammar, limits, and deferred semantic responsibilities in README
   and `docs/parser.md`. Update architecture and exact file inventories.

Success: CLI tests cover success, syntax/preprocessing errors, warnings, mixed
inputs, option conflicts, and output preservation. An API consumer traverses a
parsed module after the preprocessing session has been destroyed.

Required commit: `feat(cli): expose syntax checking through the typed parser`.

### Step 18. Close compatibility coverage and validate supported builds

1. Run the complete grammar/AST differential suite and audit every coverage row.
   Structural comparison must fail on unknown/unmapped node kinds rather than
   omit them. Compare errors by class/location expectations, not exact OTP prose.
2. Parse a recorded selection of real pinned OTP modules/headers, including macro,
   record, spec, bit-syntax, and comprehension-heavy sources. Supply explicit
   include/application/feature options and report preprocessing versus parsing
   failures separately. Account for test-only annotations and known upstream
   crashes explicitly; no unexplained blanket skips or claims of full suite runs.
3. Run C++23, selectable C++26 where supported, sanitizer, compiler-only,
   runtime-only, and full builds. Test macOS Apple Silicon, Linux x86/ARM, and
   Windows x86-family hosts using available CI/toolchains. Record the exact host/
   architecture matrix; platform completion requires execution evidence, not
   inferred portability. Unavailable required hosts remain open work.
4. Verify deterministic AST structure and stress behavior, review accidental
   duplication with preprocessing, and finish the fresh full-build quality gate.
   Compact architecture/file/memory notes and publish actual coverage/limitations.

Success: all ordinary grammar rows have native and oracle evidence, all mandatory
checks pass, no silent AST loss remains, and the host matrix distinguishes passed
jobs from pending work. Declare the full plan complete only after required host
validation is satisfied; otherwise retain an explicit partial completion status.

Required commit: `test(parser): complete compatibility corpus and build validation`.

## Final deliverable

A native parser consuming existing expanded tokens, an owned type-safe syntax AST
with usable source provenance, a documented parse-check mode, and reproducible
native/offline/live-oracle coverage. Preprocessor behavior stays protected by its
existing tests, shared mechanisms have a single implementation, and semantic
analysis receives all syntax and feature context it needs without a premature
stage-format or backend decision.
