# Phase III parser fixtures

Authored against OTP-29.1, commit `751f87b703fe5948607d08e82599ce644b772e76`.
No upstream fixture code was copied. `.erl.ast` records come from the pinned
`tests/compiler/parser/oracle.escript phase1 FILE` projection. The historical mode
name is retained for earlier fixtures; this remains a private test format.

The parameterized Phase II harness checks native ASTs and `.reject` failures
offline, and optionally replays exact-version OTP AST/acceptance/lint results.
`.erl.lint` files demonstrate parser acceptance separately from semantic validity.
Annotations and basename paths follow the earlier normalization; node categories,
record identities, field operators/order and update bases are preserved.

Step 8 covers maps and tuple/native record syntax, every reserved record name,
variable-looking names, inferred identities, ordered/wildcard assignments,
access/index distinctions, exact structural chaining restrictions and malformed
forms. Unknown record definitions remain syntax, without lookup or expansion.

Step 9 covers ordered binary segments, explicit/omitted sizes and modifier lists,
arbitrary modifier integers, unknown/duplicate modifiers, unary values, restricted
sizes, nested bits, grouped calls/maps/records, binary patterns and sigils. The
projection's historical `binary_sigil` label denotes exactly a single string
segment with default size and `[utf8]` types, whether written as a sigil or explicit
bit syntax. All other binaries project complete segment/default/modifier structure.
