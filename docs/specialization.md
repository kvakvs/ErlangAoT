# Bounded type specialization

The private backend plans representation variants only in speed mode. O0 and
`disable_type_specialization` produce no candidates; command-line integration is
still reserved for the later driver steps.

Observed call-site implementation facts supply profiles of generic or small-integer
arguments. Only exact, target-representable inferred integer singletons establish
the latter proof today. Specifications, broad `integer()` categories, unknowns and
unions remain generic. Literal values are discarded; no union products are expanded.
Source traversal and profile processing have finite budgets tied to generic IR size.

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
