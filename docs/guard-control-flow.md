# Guard grouping and boolean control flow

Pattern/guard step 8 executes guard grouping and boolean operators in the current
immediate expression subset. [Step 10](body-matches.md) adds body matches and sequences. Step 7 still authorizes every operand
before lowering, including skipped operands. [Step 9](ordered-clauses.md) adds ordered clause fallback.

Commas conjoin tests in source order. Each test must return canonical `true`; false
or another term rejects that complete alternative. Semicolons start independent
alternatives: false, non-boolean or reached semantic error continues at the next
semicolon, or the candidate mismatch edge when exhausted. Tentative head bindings
remain readable across alternatives; guards cannot create bindings. Head mismatch
runs no guard or body code. Single-clause exhaustion raises `function_clause`.

| Operator | Evaluation and result |
|---|---|
| `A andalso B` | Require boolean A; false returns false; true evaluates and returns B as a term |
| `A orelse B` | Require boolean A; true returns true; false evaluates and returns B as a term |
| `A and B`, `A or B`, `A xor B` | Evaluate both operands in source order, require both booleans, return a canonical boolean |
| `not A` | Require a boolean and return its canonical inverse |

Lazy operators use separate RHS blocks and target-word SSA joins. Their right
operand may return any admitted term. `true andalso 7` returns 7 in a body and
rejects as a final guard test; `(true andalso 7) =:= 7` succeeds as a guard.
If another boolean operator consumes the intermediate 7, its own operand check
applies. LLVM's SSA updater constructs joins; an unfinished merge is temporarily
terminated during construction, and the placeholder is removed before verification.
Join instructions retain the original operator's source location.

A reached guard error rejects the enclosing alternative, including inside nested
boolean expressions. `hd([]) orelse true` cannot recover from its argument error;
`hd([]); true` can. Infrastructure errors always use the checked failure exit and
never try an alternative. Strict boolean operators evaluate their right operand
even when the left boolean would determine the result. Qualified `erlang` strict
operator calls use the same runtime services.

In bodies, invalid strict boolean arguments raise `badarg`. An invalid lazy left
operand raises `{badarg, Value}`, matching OTP. Private `ErrorReason::badarg_value`
and the existing owned payload field represent that structured reason without
constructing a heap tuple. The checked raise service admits the actual offending
integer/owned atom/empty value, and outer invocation cleanup permits later retry.
Heap payload roots remain assigned to step 11.

The iterative AST-sized scheduling stack preserves source order and conditional
evaluation without C++ recursion. Existing parser/semantic budgets bound inputs;
wide alternatives and nested lazy expressions execute through the public CLI.
Specs remain separate from implementation facts, and all modes verify complete
LLVM batches before and after optimization.
