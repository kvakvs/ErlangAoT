The completed [pattern/guard plan](patternmatch-step20-validation.md) supports
ordered function clauses and body matches/sequences over atoms, arbitrary integers,
finite floats, tuples, lists/strings, maps, bitstrings and ordinary tuple records.
Shared rooted services provide construction, extraction, structural comparisons,
arithmetic, computed-key matching and grouped guards. See [guard services](guard-services.md)
and [binding facts](binding-facts.md) for the admitted catalog and conservative
inference contract. Other source contexts and runtime owners remain deferred.
Routine tests use project-owned OTP goldens and require no OTP installation.


# Semantic compilation analysis

Current atom support (pattern/guard step 3): literal atoms and booleans, runtime-owned
module bindings and owned host/error atoms are implemented. The module descriptor
uses ABI revision 4; the revision-2 checked call channel is unchanged. See
[runtime atoms](runtime-atoms.md) for ownership, limits and registration policy.

Default positional and project invocations now validate module and function
identity, function arities (0..255), duplicate definitions, and exports after parsing.
Exports may precede definitions; missing functions and repeated exports are errors.
Decoded quoted/Unicode names keep their exact identities. Missing or duplicate
module declarations are errors. Diagnostics retain macro/include origins, and an
error does not prevent diagnostics from later input files.

`--parse-check`, `--print-ast` and preprocessing actions retain their syntax-only
contracts. Default compilation continues through LLVM object buffers in memory;
explicit `--emit` publishes artifacts. `--print-types` runs the shared analysis
pipeline and stops before constructing LLVM state. See the [compilation contract](compile.md).

Private ABI v1 function symbols are `eav1_<module hex>_<function hex>_<arity>`.
Each name component contains lowercase hexadecimal UTF-8 bytes; arities use canonical
unsigned decimal. Separators cannot occur in encoded names, so encoding is reversible
and independent of compiler-host hashing, locale and table order.

Compilation checks every function, including unused definitions. The current subset
accepts ordered clauses with scalar/container patterns, aliases and repeated variables,
body sequences/matches, rooted constructors, checked numeric/container services and
syntactically direct local/literal remote calls. Source children evaluate in order.
Record updates/native forms, exceptions, concurrency, dynamic calls, closures and behavior-changing
attributes remain diagnosed. Type/spec metadata does not authorize runtime loads.
Executable layouts and integer bounds derive from the requested LLVM target.

Executable named parameters may repeat or form aliases. The semantic binder
assigns clause-local identities to definitions, reads and exact-equality checks.
`_` creates no binding; `_Name` is ordinary. Tentative head bindings are readable
by guards and publish to the successful body path. Body matches use RHS-first
scopes; unbound, unsafe and wildcard reads retain original locations. Whole
argument reads retain projection provenance; whole-value body assignments preserve
justified RHS facts, while extracted/unproved values stay unknown.
Repeated parameters now execute exact equality through the shared runtime service.
See [scoped bindings](scoped-bindings.md) for the step-4 boundary.

Positional inputs form one compilation batch. Each selected project target forms
its own batch with independent preprocessing sessions and declaration tables.
All ASTs remain owned until analysis finishes. Forward and nested calls resolve
within that batch by decoded module/function name and arity. Remote calls,
including self-qualified calls, require explicit exports. Missing/private callees,
duplicate module identities and direct or indirect recursion are errors.
The call graph retains a deterministic callee-before-caller dependency order for
inference. Lowering consumes these independently inferred facts, never treating
specifications as proven runtime representation guarantees.

The private semantic type graph represents all parsed type categories independently
of runtime/LLVM layouts. It retains exact singleton spellings, range endpoints,
container shapes, map field roles, function products and unresolved applications.
Graph-owned identities survive storage growth and reject foreign children. Union
joins flatten and deduplicate members, with top (`term()`) and bottom (`none()`)
identities. Analysis defaults cap the graph at 16,384 nodes, unions at 16 alternatives,
and each syntax translation at 100,000 work items. Exhaustion yields top and an
observable widening flag; it never justifies a narrower runtime representation.


Declared types are resolved after the acyclic call graph, following the
[OTP type/specification contract](https://www.erlang.org/doc/system/typespec.html)
and the pinned `erl_lint.erl`, `erl_internal.erl` and Dialyzer `erl_types.erl` sources.
The registry indexes all `-type`, `-opaque`, `-nominal`, `-export_type`, `-spec`,
`-callback` and `-optional_callbacks` metadata before resolving bodies. Local aliases
may shadow builtin names. Exact integer arithmetic resolves singleton/range/bitstring
bounds without narrowing to a host word. Record declarations supply symbolic field
contracts. Ordinary executable record defaults are selected by the separate
[record expansion](record-matching.md) pass; type metadata never evaluates them.

Remote references to batch types require exports. Missing local/batch types,
duplicate declarations/exports, malformed metadata, singleton type variables,
invalid bounds and specifications for missing functions are errors. Unavailable
external metadata produces a warning and unknown/top; it is never borrowed from
another project target. Syntax-only actions keep their broader acceptance.

Each alias or overload has independent, collision-free variable scope. Union branches
are alternatives for variable-use counting. Repeated formal names follow OTP's
last-argument substitution; repeated `when` constraints remain separate bounds.
Overloads retain their individual function products, constraints and source anchors.
These are declared contracts, not runtime guards or inferred implementation facts.

Recursive aliases remain finite named references. One-layer substitution is memoized
and bounded, leaving further recursive edges as references. Opaque bodies can expand
only inside their defining module; nominal references retain their names everywhere.
Registry source locations borrow the batch-owned AST. The driver reports graph/work
exhaustion as warning-only widening; future inference callers of the expansion API
must also inspect the graph's widening flag. Constant evaluation accepts at most
10,000 decimal digits per operand/result and shares the preprocessor's bounded shifts.
Local inference now owns an independent type graph. Integer expressions retain
singleton values; parameters retain top-valued types plus exact result/argument
relations for identity and projection functions. Missing or partial specifications
do not reduce precision, and incorrect specifications cannot narrow these facts.
Iterative traversal has a batch work budget; exhaustion returns top and discards
relations. Node exhaustion widens constants to top. Resolved callee-before-caller
order propagates constants and freshly instantiated parameter relations through
nested local and remote calls. Different calls never share mutable type variables.

Specifications remain advisory: known integer results or arguments excluded by all
overloads produce located warnings, never runtime guards. Membership compares exact
decimal bounds and respects aliases, recursive budgets, opaque and nominal barriers.
Unknown values, constrained signatures and unresolved alternatives are inconclusive;
the checker deliberately does not promise full success typing or contract validation.
`--impldebug 23` reports escaped function summaries on stderr, with unknown inputs,
singleton results, parameter relations and batch widening. Ordinary `--verbose`
does not enable these summaries. Steps 24–27 expose the same facts under their own
debug prefix as lowering inputs. Private generic lowering consumes the side tables;
public type inspection and CLI artifact publication remain later plan steps.

`--print-types` reports declarations independently of implementation inference.
Function lines identify whether a specification exists, list conservative inputs
and results, and retain zero-based argument relations for identity/projection.
Expression facts include logical locations and clause/local identities for reads.
Module sections follow input order, with project target context; recursive aliases print as finite symbolic
references. Unknown inferred facts are labeled explicitly, as are graph widening
and display limits. The report is human-readable, not a stage interchange format.
Warnings (including contradictory specs) remain on stderr and do not prevent reports;
semantic errors stop the affected batch before reporting or LLVM construction.

The [admitted guard catalog](guard-services.md) includes checked
`is_integer/3`, qualified calls and top-level legacy tests; process/node and native
record identities retain explicit capability diagnostics.

[Binding facts](binding-facts.md) describes successful body assignment/alias
propagation, conservative extraction facts, checked-load dominance and the shared
inference/specialization budget fallback. Specs remain separate from these proofs.
