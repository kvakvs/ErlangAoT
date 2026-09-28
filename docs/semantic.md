# Semantic compilation analysis

Default positional and project invocations now validate module and function
identity, function arities (0..255), duplicate definitions, and exports after parsing.
Exports may precede definitions; missing functions and repeated exports are errors.
Decoded quoted/Unicode names keep their exact identities. Missing or duplicate
module declarations are errors. Diagnostics retain macro/include origins, and an
error does not prevent diagnostics from later input files.

`--parse-check`, `--print-ast` and preprocessing actions retain their syntax-only
contracts. Successful compilation still emits no executable or other artifact.
Lowering and public type inspection are
later steps of the [compile plan](../.agents/04-compile.md).

Private ABI v1 function symbols are `eav1_<module hex>_<function hex>_<arity>`.
Each name component contains lowercase hexadecimal UTF-8 bytes; arities use canonical
unsigned decimal. Separators cannot occur in encoded names, so encoding is reversible
and independent of compiler-host hashing, locale and table order.

Compilation checks every function, including unused definitions. The current subset
accepts one clause with variable/wildcard arguments and one expression: an ABI-small
integer, parameter reference, or syntactically direct local/literal remote call.
Nested call arguments are checked. Explicit negative integers are supported;
arithmetic, atoms, heap values, matching, guards, exceptions, concurrency, dynamic
calls, closures and behavior-changing attributes are diagnosed. Type/spec metadata
is symbolic and does not enable executable syntax. Current CLI bounds are native;
the analysis API accepts explicit 32/64-bit target bounds for later target setup.

Each named parameter must be distinct. `_` consumes an argument position without
creating a binding; `_Name` is an ordinary named variable. Body reads retain the
original argument index in semantic side tables. Reading `_`, using an unbound
name, or repeating a named parameter is rejected before lowering.

Positional inputs form one compilation batch. Each selected project target forms
its own batch with independent preprocessing sessions and declaration tables.
All ASTs remain owned until analysis finishes. Forward and nested calls resolve
within that batch by decoded module/function name and arity. Remote calls,
including self-qualified calls, require explicit exports. Missing/private callees,
duplicate module identities and direct or indirect recursion are errors.
The call graph retains a deterministic callee-before-caller dependency order for
inference. Executable body inference is now implemented; code emission remains pending.

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
contracts; their defaults remain unevaluated and executable records remain unsupported.

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
