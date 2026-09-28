# Semantic compilation analysis

Default positional and project invocations now validate module and function
identity, function arities (0..255), duplicate definitions, and exports after parsing.
Exports may precede definitions; missing functions and repeated exports are errors.
Decoded quoted/Unicode names keep their exact identities. Missing or duplicate
module declarations are errors. Diagnostics retain macro/include origins, and an
error does not prevent diagnostics from later input files.

`--parse-check`, `--print-ast` and preprocessing actions retain their syntax-only
contracts. Successful compilation still emits no executable or other artifact.
Call resolution and type analysis are
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
