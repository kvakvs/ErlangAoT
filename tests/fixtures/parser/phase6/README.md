# Parser completion corpus

Baseline source: OTP 29.1, commit `751f87b703fe5948607d08e82599ce644b772e76`.
Live projection records were produced with installed OTP 29.0.5. CTest never
regenerates committed records.

`coverage.tsv` records a measured reduction witness for every one of the 423
inventoried productions: 344 ordinary productions observed and 79 SSA annotation
productions explicitly excluded. `coverage.escript` instruments actions in a
temporary copy of the pinned grammar, generates/compiles it with Yecc, and feeds
preprocessed fixture tokens to it. It scans all authored positive/rejection
fixtures. A production reached before an intentional builder error still counts
as an observed reduction; acceptance and AST shape are checked by separate
native/live-oracle tests. Function-clause/badmatch exceptions are allowed only
for the named `.builder-reject` fixtures. Other unexpected exceptions fail.

Each ordinary row has one deterministic witness, preferring a positive input.
`historical.cmake` verifies row identity, production spelling, fixture existence
and permitted exclusions even without the source checkout. The instrumented
test recomputes the report and compares it exactly. Reduction coverage does not
claim semantic-action branch coverage or full upstream test-suite execution.

`coverage.erl` closes empty map/tuple/qualified-record types and multi-field record
types. `typed_attribute.reject` covers a typed record body under a non-record
attribute name. `historical/*.ast` completes native/live structural comparison
for all 14 successful original seed sources. The original checksummed records
remain unchanged; `bad.erl` remains a located source rejection.

`otp.tsv` pins ten real sources by path and SHA-256. They cover stdlib lists/maps/
sets/scanning and compiler SSA/assembly/record/type/bit syntax. The corpus test
verifies the checkout revision and clean relevant dependencies, then separately
checks preprocessing and parsing and compares two complete printed trees. It
uses compiler/src as an include path, explicit stdlib/kernel application roots,
maybe_expr enabled, compr_assign disabled, and
`COMPILER_VSN="parser-compatibility"` (normally injected by OTP's build).
The generated `tests/corpus/corpus.tsv` records per-stage success and tree hashes.
Tree hashes are local determinism evidence, not a stable interchange format.

Set `ERLANG_AOT_OTP_SOURCE_ROOT` to the pinned checkout. Without it only the two
source-dependent tests explicitly skip; authored fixtures, integrity checks and
installed-OTP oracle comparisons still run. No source-dependent skips occurred
in the recorded macOS validation. See `docs/parser-validation.md` for the matrix.
