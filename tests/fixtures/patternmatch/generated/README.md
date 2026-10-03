# Project-owned native oracle fixtures

These checked-in corpora are the inputs and expected results used by ErlangAoT's
native regression tests. Routine native tests verify the recorded hashes, copy
the fixtures to their build directory, and compare ErlangAoT execution with
`expected.txt`. They do not extract OTP source or run OTP to recalculate results.

Each directory contains expected values/error reasons, fixed call inputs and a
manifest. Locally authored Erlang sources live in the corresponding
[`../fragments/`](../fragments/README.md) directory. Native corpora also retain
`project.toml` where used. `calls.txt` contains native inputs and `calls.term`
retains equivalent Erlang terms where used by the oracle. The fixed atom
consumer selects its calls directly. Float results retain exact IEEE binary64 bits;
arbitrary integers retain their mathematical values. Manifests hash the local
source separately from OTP observations; no copied OTP source is committed.

The nineteen corpora retain 67,634 native expected results plus 106 semantic
acceptance rows. All native corpora exercise both CLI drivers in all four policies.
The closure corpus adds deterministic nested stress and checks every manifest and
all 81 guard catalog mappings; see [final validation](../../../../docs/patternmatch-step20-validation.md).
Bitstring transport preserves the exact bit count and packed MSB-first bytes,
with zero unused low bits in the final byte; it never serializes buffer identity.

Regeneration is an explicit maintainer action, from the repository root:

```powershell
python tests/compiler/patternmatch/regenerate.py --otp references/otp --escript 'C:/Program Files/Erlang OTP/erts-17.1/bin/escript.exe'
```

Use `--corpus integers` (or another directory name) to refresh only one corpus.
The refresher verifies the pinned checkout and evidence manifests, stages fixed
local fragments and call inputs, executes the selected OTP oracle, and writes
observations with hashes, oracle version and reference revision. Source files
are never generated or copied from OTP. Edit local fragments deliberately before
refreshing their observations; source and input inventory hashes must agree.
Review and commit those changes. Never regenerate expectations merely to make
a failing native comparison pass. Temporary generation directories stay under
the ignored `build/` directory.

`--check` regenerates into temporary storage and detects drift without changing
committed expectations. Configure with `-DERLANG_AOT_OTP_AUDITS=ON` to enable
separate live grammar, semantic-acceptance, provenance and fixture drift audits.
The default OFF leaves building and testing independent of OTP and its checkout.
Historical validation records retain their original live-oracle methodology.
