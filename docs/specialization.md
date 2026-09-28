# Bounded type specialization

The private backend plans representation variants only in speed mode. O0 and
`disable_type_specialization` produce no candidates; command-line integration is
still reserved for the later driver steps.

Observed call-site implementation facts supply profiles of generic or small-integer
arguments. Only exact, target-representable inferred integer singletons establish
the latter proof today. Specifications, broad `integer()` categories, unknowns and
unions remain generic. Literal values are discarded; no union products are expanded.
Source traversal and profile processing have finite budgets tied to generic IR size.
Missing expression facts after inference work exhaustion also stay generic; losing
analysis precision never rejects otherwise supported compilation.

The benefit recognizer accepts exact low-tag comparisons of ABI argument loads in
the entry block before any side effects. Profiles drop constraints for arguments
with no removable check. Equal resulting profiles are deduplicated. Selection is
ordered by module, function symbol and profile, independent of host addresses.
Limits are three variants per function, 32 per module and 128 per compilation target.
Generic bodies are always retained. Estimated extra instructions include the clone,
all tag guards, branch/call/return dispatch and a generic fallback; both function
and module growth must remain within their original generic instruction counts
(at most 2x total). Budget exhaustion skips variants without rejecting the program.

The current executable source subset has no removable representation checks.
Constants, identity/projection and resolved direct calls therefore receive no
variants even at O2. This is deliberate: specializing a tagged identity would
increase code size without removing a dynamic operation. Policy tests use measured
check-count inputs to exercise limits that current source syntax cannot reach.
Real-source tests verify that inference does not invent a benefit.

The guarded lowering stage clones only proposed profiles with matching implemented
checks. LLVM cloning and simplification utilities replace those checks under the
profile proof; no unchecked unboxing, source-spec assumptions or arithmetic flags
are added. Public entries retain the same context/argument-array/tagged-result ABI.
A bounded dispatcher checks every constrained low tag and forwards both pointers
unchanged to a variant or the original generic body. Current selection always uses
guards; no call bypasses them on a declared type alone.

Each function's draft is measured before publication. Actual clone instructions
plus all dispatch/fallback instructions must fit the original function and remaining
module growth budgets. Excess drafts are erased; the program and its generic body
remain valid. Installed wrappers preserve the external symbol and descriptor
references; original bodies and variants become internal functions. The plan keeps
separate counts for estimated candidates, installed variants and discarded drafts.

Focused LLVM fixtures are necessary because source guards are still unsupported.
They run after genuine parsing, semantic analysis and registration emission, then
replace selected test bodies with repeated, exact tag checks. Native Clang-linked
runtime consumers compare guarded execution with untouched generic bodies over
small-integer endpoints, negative/zero values and empty tuple/list inputs, including
mixed argument pairs. A deliberately stale estimate proves actual-growth rollback;
the rejected function is also executed through its normal registered entry.
Cross-width checks verify 32-bit IR/objects; native execution is validated separately.
These fixtures do not claim new Erlang source support or an optimization pipeline.
