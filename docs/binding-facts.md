# Binding facts and representation proofs

Inference indexes validated clause-local binding identities once. Whole-value body
assignments and pattern aliases copy their successfully matched RHS fact and any
justified original-argument relation. Thus `Y = 42, Z = Y, id(Z)` can infer 42,
and `Y = X, id(Y)` retains the relation to X. New definitions are published only
after RHS analysis; exact checks never overwrite an earlier definition's fact.
Clause IDs keep candidates disjoint. Function joins retain only common argument
relations and widen differing types.

Extracted tuple/list/map/bitstring/record fields, guard refinements, service results
and missing/unproved facts remain `term()` with no original-argument relation.
This is deliberate conservatism: these values are handled without an argument-index
lookup or an invented whole-argument projection. Specifications stay in the separate
contract graph and cannot create a representation proof. Neither declared integer
inputs nor successful scalar predicates authorize raw compound heap loads.

Binding-index construction and alias publication consume the existing shared
inference work ceiling. Exhaustion loses precision and uses the generic compiler
path. The separate specialization limits stay unchanged: 3 variants per function,
32 per module, 128 per target, bounded profile work and at most twice measured
generic IR. Actual draft growth is checked before replacing generic code. No new
integer-check removal is introduced without an entry/control-flow proof.

Lowering indexes read identities once per function, while each candidate retains its
own SSA environment. Shared read metadata does not share values. Representation and
ownership checks remain in checked runtime services; generated source IR contains
no integer-to-heap-pointer conversion. Each fallible service's output is loaded only
on its checked successful continuation, and shape checks precede dependent match
extractions. Infrastructure failure checks and roots remain on the common boundary.

The owned facts corpus pairs annotated and unannotated versions across all admitted
representations, including nested allocation, computed keys/sizes and failed
candidates. All 822 OTP outcomes pass in four policies and both CLI modes. Public
`--print-types` and both IR modes handle every kernel at 32/64-bit target widths.
CFG dominator analysis records 976 observations of checked-output load ordering,
shape-before-extraction and bit-cursor ordering. Native execution is Windows x64;
32-bit IR is validation rather than foreign execution.

Private real-source checks inject inference and artifact ceilings that the CLI does
not expose. Compound/map/bitstring/record/float source still verifies, optimizes and
emits through generic inference fallback; byte ceilings clear all staged outputs.
Existing variant/work/growth rollback tests are retained. See the step-19 validation
record for the fresh full gate.
