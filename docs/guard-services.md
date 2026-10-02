# Guard service catalog on admitted terms

The executable guard catalog follows the 81 signatures in the pinned OTP 29
`erl_internal.erl` tables. [The signature matrix](../tests/fixtures/patternmatch/guards.tsv)
and [the owned audit](../tests/fixtures/patternmatch/generated/guard_catalog/manifest.json)
identify resolver, lowering, runtime owner and named executable helpers for every
row. Ordinary BIFs are tested unqualified and qualified; operator BIF calls use
explicit `erlang` qualification. Existing numeric, container, map, bitstring and
record corpora retain deeper representation-specific boundary coverage.

`is_integer(Value, Lower, Upper)` validates **both bounds as integers first**.
Invalid bounds raise `badarg` in bodies and reject the reached guard alternative,
even when Value is not an integer. Otherwise it returns true exactly for an
integer in the inclusive interval; reversed bounds and nonintegers return false.
Arbitrary integers use the existing checked comparison services. Bound validation
and candidate classification precede range comparisons; every service retains
its infrastructure failure and root handling.

Guard constructors and map updates share rooted body services. Reached badmap,
badkey, numeric, access or construction errors reject the enclosing alternative.
Resource, ownership and internal failures terminate through the existing channel.
No guard-specific term representation or unrooted extraction is introduced.

Legacy aliases apply only to top-level tests (parentheses preserve this context).
Legacy `record/2` ignores suppression/shadowing of its obsolete name, while a local
`is_record/2` prevents the alias. Other aliases require the old name to remain
authorized and the modern name to have no local or unrelated imported owner;
modern auto-import suppression alone does not prevent the legacy test. Explicit
modern erlang calls remain independent of local shadowing. The installed oracle
crashes during SSA conversion for unrelated modern imports behind legacy scalar
aliases; the project rejects this unauthorized owner explicitly. This is a
compiler-oracle limitation, not a claim of an ordinary OTP lint diagnostic.

`self/0`, `node/0,1` require F07/F22/F26; `is_record/1` requires F17 native records.
These four signatures stay legal but unavailable, including qualified calls and
unreachable operands. The owned semantic corpus tests each combination. Predicates
for function/pid/port/reference values classify the admitted domain as false;
these values cannot yet be constructed/admitted. No positive coverage is claimed
for those unavailable representations.

The additional native corpus contains 5,033 OTP outcomes in all four O0/O2 and
specialization policies, using positional/project batches and local/remote calls.
It retains complete `guard_SUITE:is_integer_3_guard_1..8` and their identity helpers,
supported complete `bif_SUITE:min_max/1` helpers, and complete helpers called from
`map_SUITE:t_guard_bifs/1`. Conversion kernels explicitly adapt
`bif_SUITE:trunc_and_friends/1`'s generated template: ordered guards replace if/try
and preserve conversion/equality relations; separate body catalog cases cover
wrong-type errors. Common Test/meta-generation itself is not executable coverage.

All normal tests use owned pregenerated source and answers. OTP remains an
explicit regeneration/audit dependency only. See the step-18 validation record.
