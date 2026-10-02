# Body matches and ordered sequences

Function bodies evaluate their expressions in source order and return the final
value. Every fallible call or service is checked before the next expression.
The capability, call, inference, atom and inspection walks include every body
expression, including unused and unreachable work.

An expression match evaluates its RHS once, then supplies that saved word to the
same normalized match plan used for function heads. New binding IDs refer to the
candidate word on the success continuation. Existing IDs are exact-equality
constraints and cannot be assigned a new value. Head values used only by later
match constraints are loaded even when there is no ordinary variable read.

Chains evaluate their inner RHS match first. A parenthesized compound pattern
shares one candidate across all its alias operands. RHS-created bindings are
available to the enclosing pattern according to the semantic binding analysis.
Matching returns the original RHS word, including when the LHS is a wildcard.
Patterns are validated as patterns; their folded constants are not re-evaluated
as ordinary body expressions.

A failed body match raises `error:{badmatch, RHS}` through the existing checked
call channel, retaining the immediate payload's ownership. It does not continue
the sequence or retry a later function clause. Nested callers preserve the first
failure. Heap payload admission and roots remain assigned to steps 11–12.

The admitted match domain remains variables, wildcards, aliases, small integers,
atoms and canonical empty lists/tuples. This delivers the sequence/match slice of
F16. Blocks, case/if, exceptions and other source control contexts retain their
separate capability gates. See [step 10 validation](patternmatch-step10-validation.md).
