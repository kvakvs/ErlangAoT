# Ordered function clauses

Every function candidate is analyzed, including later unreachable clauses and
unexported functions. The shared guard/body root walk feeds call resolution,
inference, atom collection, type inspection and specialization observations.
Heads retain their normalized pattern analysis and clause-local binding IDs.
Recursive call components remain rejected by the existing F21 boundary.

LLVM lowering creates each candidate in source order with a fresh SSA value and
binding environment. A head mismatch or rejected guard reaches the next clause,
which reloads the original arguments. Guard alternatives retain their existing
short-circuit and semantic-error behavior. A selected body returns directly;
body errors and runtime infrastructure failures leave the function without
trying a later candidate. All reachable exhausted paths share one
`function_clause` exit. Unconditional projection bodies keep compact IR.

Inference analyzes all guard/body expressions and joins the final results of
all clauses conservatively. An argument projection survives only when every
candidate returns that same original argument. Source specifications never
narrow these facts, and unreachable bodies are still validated and included in
the conservative summary.

This delivers F15 for the admitted immediate pattern/guard domain. [Body sequences and matches](body-matches.md) are delivered by step 10; nonempty containers and recursive execution retain their separately owned gates.
Executable evidence is recorded in [step 9 validation](patternmatch-step9-validation.md).
