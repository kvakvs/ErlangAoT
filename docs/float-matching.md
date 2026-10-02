# Finite floats and numeric matching

Source expressions, function heads and body matches admit IEEE binary64 literals.
The compiler preserves the scanner's bits and passes eight network-order bytes
to a checked C++ runtime service. LLVM sees target-width rooted words, not a
platform-dependent floating argument convention. Runtime storage is immutable,
indexed by its owning process heap, and uses the existing reservation/publication
rollback and generated root scopes. Host access rejects foreign or expired values.
NaN and infinities are rejected before allocation or publication.

`+`, `-`, `*` preserve exact integer arithmetic when both operands are integers;
a float operand selects checked binary64 arithmetic. `/` converts both operands
to binary64 and checks the divisor and finite result. Integer-only operations
continue to reject floats. Unary signs and `abs/1` support both numeric families.
`float/1` rounds integers once to nearest binary64, ties to even, and rejects
overflow; `round/1` uses ties away from zero. `trunc/1`, `floor/1`, and `ceil/1`
return arbitrary integers without host-width narrowing. Integer inputs to these
integer-result conversions are retained. Runtime arithmetic assumes the default
native IEEE rounding environment; generated LLVM carries no fast-math promises.

Exact equality and repeated-variable/literal matching distinguish integers from
floats and distinguish positive from negative float zero, as OTP 29 requires.
Numeric equality and ordinary ordering treat equal numeric values alike. Mixed
comparison uses the exact truncated float integer plus its fractional remainder;
it never rounds an arbitrary integer into a float first. The same rules apply
recursively to tuples and lists. `min/2` and `max/2` return their first operand
on numeric ties. General numeric comparison has bounded integer intermediates.

Wrong operands, division by zero, or nonfinite arithmetic produce `badarith` in
bodies. Conversion BIFs and `abs/1` produce `badarg` for invalid operands/ranges.
Reached semantic errors reject the enclosing guard alternative. Allocation,
ownership, and work-limit failures retain their infrastructure status and stop
selection; they cannot become false guards. Existing operation IDs are preserved
and new services append IDs. As with the other internal services, generated
objects require their matching compiler/runtime build.

The native oracle workflow uses exact binary64 bit transport, including subnormals
and signed zero, and runs all four optimization/specialization policies in both
CLI modes. It preserves `float_SUITE:pc/3` unchanged and labels the ordered-clause
adaptation of `beam_type_SUITE:float_compare/1`. Host tests separately cover
nonfinite rejection, ownership, growth, expiration, malformed literal transport,
allocation-ordinal rollback, and successful retry. Foreign object emission is not
foreign native execution. Collection and cross-process copying remain deferred.
