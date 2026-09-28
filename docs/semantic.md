# Semantic compilation analysis

Default positional and project invocations now validate module and function
identity, function arities (0..255), duplicate definitions, and exports after parsing.
Exports may precede definitions; missing functions and repeated exports are errors.
Decoded quoted/Unicode names keep their exact identities. Missing or duplicate
module declarations are errors. Diagnostics retain macro/include origins, and an
error does not prevent diagnostics from later input files.

`--parse-check`, `--print-ast` and preprocessing actions retain their syntax-only
contracts. Successful compilation still emits no executable or other artifact.
Function-body capability checking, parameter/call resolution and type analysis are
later steps of the [compile plan](../.agents/04-compile.md).

Private ABI v1 function symbols are `eav1_<module hex>_<function hex>_<arity>`.
Each name component contains lowercase hexadecimal UTF-8 bytes; arities use canonical
unsigned decimal. Separators cannot occur in encoded names, so encoding is reversible
and independent of compiler-host hashing, locale and table order.
