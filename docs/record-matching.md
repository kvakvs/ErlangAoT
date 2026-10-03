# Tuple record expansion

Pattern/guard step 17 implements ordinary tuple records in function heads, body
matches, value construction, field access, field indices and guard tests. Records
use the existing tuple representation and checked tuple services. Native,
qualified/imported and inferred record forms, updates and `record_info/2` remain
outside this executable slice.

Preprocessed declarations provide a tag, ordered fields, defaults, positions and
owned source locations. A declaration must precede its use, including uses in
another declaration's defaults. Duplicate declarations/fields/initializers,
unknown fields, forward/self references and invalid wildcard fields are located
semantic errors. Defaults cannot capture or create ordinary named variables;
closure scopes remain a separate deferred capability.

Construction evaluates fields in declaration order. An explicit initializer wins;
otherwise a wildcard initializer is evaluated separately for that omitted field;
otherwise the declared default is evaluated, or atom `undefined` is supplied.
Each result is captured immediately before another use of shared default syntax
can evaluate. This preserves single evaluation and distinct rooted allocations.
All executable walks include the selected defaults, including call resolution,
guard legality, inference and atom registration. Each record use charges all
expanded fields against the shared 1,000,000-unit semantic work budget.

Patterns check tuple arity and the tag, then constrain only supplied fields.
Omitted pattern fields impose no default-value constraint. A wildcard field fills
only omitted positions; repeated names still require exact equality. Expansion
uses the shared tuple shape/extraction match-plan nodes and caller continuations.
`#r.field` is the one-based tuple position, with the tag at position 1.

Access evaluates its operand once and checks ownership, arity and tag before
extracting a rooted field. A shape/tag mismatch rejects a guard; in a body it
raises `error:{badrecord,Value}` with the existing owned failure payload. An
infrastructure failure terminates the invocation, rather than selecting another
clause or guard alternative.

Literal `is_record(Value,r)` uses the visible declaration's exact tuple arity.
A dynamic tag in an ordinary body uses the BIF's any-positive-arity tuple rule.
`is_record/3` validates an atom tag and a small integer arity; negative/zero arities
return false, noninteger/nonatom and boxed integer arities cause `badarg`. An atom
third argument is OTP 29's native-record query and returns false for the admitted
tuple domain. Guard tags and third arguments require literal atom or integer
syntax; unary/arithmetic size expressions are illegal. Legacy top-level
`record/2` follows the ordinary declared-record test.

The retained [record corpus](../tests/fixtures/patternmatch/generated/records/manifest.json)
contains 1,025 OTP observations and 29 semantic cases from
[local record fragments](../tests/fixtures/patternmatch/fragments/records/answer.erl).
The local include supplies typed/default/nested record examples without copied
OTP source. Supplemental IR call counts check evaluation obligations; record
updates and process-dictionary counters remain deferred. Parsing the original
suite and data module is separate, transient syntax evidence only.
The installed OTP 29.1.1 loader rejects bytecode for a guard with a huge literal
arity, despite lint acceptance; that boundary has semantic evidence and dynamic
BIF coverage, and is not claimed as successful native-versus-OTP guard execution.
