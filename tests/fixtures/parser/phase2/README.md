# Phase II parser fixtures

Authored probes against OTP-29.1, commit
`751f87b703fe5948607d08e82599ce644b772e76`; no copied upstream code.
Each step directory contains accepted `.erl` programs, pinned `.erl.ast` structural
records, and parser-rejected `.reject` programs. Generate records with the pinned
`tests/compiler/parser/oracle.escript phase1 FILE` adapter; its mode name is retained
for compatibility with Phase I. `accept FILE` classifies parser acceptance without
running lint. The native dump must reject every `.reject` file and exactly match
every accepted record. Live replay is optional and requires the exact OTP version.

Projection normalization removes annotations and grouping, flattens list cons
spines, and shortens implicit file paths to basenames. It preserves payload kinds,
Unicode values, arbitrary integers and binary64 bits. Unknown nodes fail the dump.
Native tests separately assert richer source metadata and AST ownership invariants.

Step 5 covers literals, variables, aggregates, grouping, concatenation and sigils.
Sigils use OTP's decoded strings: s/S remain strings, empty/b/B become UTF-8 binaries.
Malformed separators, tails, prefixes, suffixes and sigil concatenation are rejected.

Step 6 covers every operator, representative cross-precedence pairs in both orders,
catch, arbitrary remote expressions, dynamic/chained calls and malformed boundaries.
Operator identities and child order are retained exactly in the projection.
