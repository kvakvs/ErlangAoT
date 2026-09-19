# Architecture

- Specifications/callbacks retain local or qualified names, first-signature arity,
  overloads, function products/results and modern/legacy subtype constraints.
  Parsing rejects malformed builder shapes while deferring overload agreement and
  implementation/type consistency to semantic passes. Contextual helper names are
  interpreted only at attribute dispatch; ordinary atoms retain their meanings.

- Type syntax has a separate checked `TypeId` arena, type-only precedence entry
  points and exhaustive visitors. Type operators are retained, never evaluated.
  Applications preserve parser builtin/local/remote classification; declarations
  preserve alias/opaque/nominal categories and variable parameters. Record field
  types reuse declaration/default parsing while keeping expression/type IDs distinct.

- Attribute parsing separates literal `TermId` data from default/equiv `ExprId`
  syntax. Literal arenas participate in transactions, budgets and ownership checks;
  temporary expression syntax is reclaimed after normalization. OTP-shaped generic
  attributes remain data, with explicit export/import/record/documentation payloads.
  The normalizer reuses private exact value comparison, numeric operations and the
  shared literal binary encoder; it never invokes ordinary calls or guard evaluation.

- CMake/C++23 host compiler `erlangaot`, with selectable C++26. Separate placeholder
  runtime archive and ABI interface; no compiler/runtime linkage or LLVM dependency yet.
- `erlang_frontend` owns sources, lexing, directive grammar, and semantic preprocessing.
  Compiler-private Boost >=1.90 Parser and Multiprecision headers (installed/Homebrew
  or local checkouts; standalone Parser stays pinned to 1.90); runtime-only builds
  skip discovery. Character combinators plus a bounded project-token expression cursor.
- `DirectiveReader` is the syntax-only reader; `PreprocessorSession` streams expanded
  forms/file attributes and diagnostics. Source ownership and physical spelling survive
  sessions; logical locations and expansion/include provenance remain separate.
- Macro keys distinguish object and arity forms. Static dependencies and dynamic ancestry
  detect cycles while finite nested arguments remain valid. Raw substitution/stringification
  preserve source order. Include frames own scanners/branches/logical mappings; macros,
  features and prefix state are shared within one module only.
- Conditions use a closed guard AST/value evaluator with arbitrary integers, exact keys,
  Erlang term order, bitstrings and short-circuit semantics after full syntax validation.
  Literal-term parsing is reused for initial definitions and diagnostic directives.
- Pinned OTP 29.1 feature metadata controls incremental keywords and query definitions.
  Includes use injectable filesystem/environment and explicit application directories.
- CLI `--preprocess-check` reports diagnostics without outputs; `--print-pp` streams
  expanded UTF-8 Erlang source. `printing/` shares canonical token formatting with macro
  stringification, preserving sigil bodies and record-access dot boundaries. Compilation remains
  explicitly unavailable. Stage interchange, full parsing/backend/runtime remain deferred.
- CTest combines native semantic/resource/location tests, CLI regressions, offline token
  records, installed OTP 29+ live comparisons, and copied OTP-header smoke tests.
  Native compiler test configuration requires a working host escript >=29, preferring
  Homebrew on macOS; explicit ERLANG_AOT_ESCRIPT overrides discovery. Other builds skip it.
  macOS arm64 is the validated host; Linux/Windows validation remains outstanding.
- Required fresh full-build `check-quality`: Lizard CCN <=10 and clang-tidy cognitive <=10,
  analyzer/bugprone/performance warnings as errors. No threshold relaxation or suppression.
- `references/otp` is an ignored pinned research checkout, not a build dependency.
  See `docs/preprocessor.md`, `.agents/01-pp-otp-tests.md`, and `00-plan.md`.
- Parser Phase I: pinned grammar/action inventories, raw/epp/lint references and native
  AST parity tests. Shared cursor/syntax/diagnostics/infix metadata also serve preprocessing.
- Typed AST: move-only module arenas, owner/generation-checked expression/pattern/form
  IDs, owned origins and rollback transactions. Module/file attributes and complete
  function clauses support scalar/variable, tuple/list/group, sigil, operator, match/catch,
  call and remote expressions. Restricted pattern roots reuse expression payloads;
  nested containers remain grammar-permissive. PatternCandidate explicitly defers
  pattern validation in future permissive positions. Guard alternatives/conjunctions
  and clause bodies are checked nonempty; semantic binding/guard legality is deferred.
- Maps preserve association/exact fields and construction/update bases. Records retain
  local unresolved, qualified native, or inferred identities, ordered assignments,
  field accesses and indexes. Structural postfix grammar restricts chaining separately
  from general operators/calls; no record layout lookup or expansion occurs in parsing.
- Blocks/case/if/receive retain nonempty bodies, distinct guard-only and candidate-pattern
  branch clauses, and explicit receive timeouts. Shared clause payloads live below expression
  nodes; bounded parsing reuses function sequences/guards and recovers transactionally.
- Funs reuse checked restricted clauses and retain recursive names or typed local/remote
  references. Try retains optional of/catch/after parts, restricted catch reasons and
  omitted class/stacktrace syntax. Maybe bodies alone admit conditional matches; their
  candidate patterns and optional else branches defer semantics and retain feature context.
- List/map/binary comprehensions reuse aggregate prefixes and preserve multiple list/map
  templates, candidate generators, strictness and zipped-versus-sequential qualifier groups.
  Match filters retain compr_assign feature context without performing binding or lowering.
- Bitstrings retain ordered value/size/type segments and modifier integer parameters.
  Restricted bit_expr/expr_max entries protect segment delimiters; grouping admits full
  expressions. Binary sigils use the same string/UTF-8 segment representation. Type/size
  legality and runtime layout are deferred; nesting and node limits cover all these paths.
- `ParserSession` consumes expanded forms/diagnostics, bounds tokens/nodes/messages
  and recursive nesting, latches failure and recovers at form boundaries. Immutable feature snapshots survive
  preprocessing; final module features are recorded at EOF. `--print-ast` consumes this
  pipeline and prints recovered forms with diagnostics on stderr; combined PP/AST printing
  uses one pass. Exhaustive visitors in `printing/tree*` stream an iterative, role-labelled
  tree with compact scalar fields and bounded indentation. No diagnostics-only parse-check CLI yet.
