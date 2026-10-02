# Exact integers in patterns, bodies and guards

Integer literals and results are exact in the admitted function-clause/body-match
slice. A selected target's signed 28-bit or 60-bit payload uses the existing
immediate encoding. Larger values use immutable, process-owned sign/magnitude
words: a boxed header, a zero/one sign word, then least-significant native words
first. Zero and every representable small value normalize to an immediate.
No host-width conversion is used for arbitrary literal text.

`TermFactory::integer` and `integer_decimal` publish through the stable heap
reservation/index transaction. Persistent cells contain only words; Boost
multiprecision intermediates own temporary arithmetic storage and are destroyed
before service return. Host handles pin their storage, reject foreign ownership
and deny access after context expiration. `integer_decimal()` is lossless;
`integer_value()` reports out_of_range when the result does not fit int64.

Generated addition, subtraction and multiplication first check both immediate
tags, then compute in twice the target width and check explicit target payload bounds.
Every failure of that proof reaches the checked exact runtime operation. A result
is encoded only on the proven fast path. Division/remainder, infinite two's
complement bitwise operations, signed shifts, unary signs, complement and abs use
the same rooted service boundary. Division truncates toward zero; remainder has
the dividend's sign. Negative shift counts reverse direction and arithmetic right
shifts sign-extend. Excessively large right shifts saturate to zero or minus one.

Wrong operands and zero divisors raise badarith in bodies; abs/1 uses badarg.
Reached semantic errors reject their guard alternative. Allocation, ownership and
work-limit failures enter the checked infrastructure channel and stop all guard
and clause fallback. A later independent invocation starts with clean roots and
failure state. Operands are evaluated in source order before the operation.

Runtime results have a 1,000,000-bit magnitude ceiling. Multiplication and shifts
check growth before allocating a result; additions need at most one transient
extra bit. Decimal input/literal text is limited to 10,000 characters, including
an optional sign. The parser and normalized-pattern constant evaluator retain
their own documented budgets. Stable process backing remains separately bounded
(64 MiB by default). These ceilings produce resource_limit infrastructure failures;
they are not advertised as OTP's implementation-dependent system_limit threshold.

Literal patterns, repeated names, aliases and nested tuple/list equality use exact
numeric values, including independently allocated equal bignums. Ordinary numeric
ordering and min/max compare exact integers. is_integer/is_number and legacy
integer tests include bignums; is_function/2 accepts any nonnegative integer arity
and returns false for the currently admitted non-function domain. Huge element/2
indices produce badarg without narrowing or hiding allocation errors.

Floating point and mixed numeric semantics belong to step 14. No collection,
cross-heap graph copying or scheduling is introduced here. See
[validation](patternmatch-step13-validation.md), [memory](runtime-memory.md) and
[generated roots](generated-roots.md).
