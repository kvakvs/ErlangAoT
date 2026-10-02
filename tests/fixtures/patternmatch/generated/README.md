# Project-owned native oracle fixtures

These checked-in corpora are the inputs and expected results used by ErlangAoT's
native regression tests. Routine native tests verify the recorded hashes, copy
the fixtures to their build directory, and compare ErlangAoT execution with
`expected.txt`. They do not extract OTP source or run OTP to recalculate results.

Each directory contains complete Erlang modules, expected values/error reasons,
and a manifest. Native corpora retain `answer.erl` and `client.erl`, plus
`project.toml` where used. `calls.txt` contains native inputs and `calls.term`
retains equivalent Erlang terms where used by the oracle. The fixed atom
consumer selects its calls directly. Float results retain exact IEEE binary64 bits;
arbitrary integers retain their mathematical values. Original OTP license
notices and helper/adaptation provenance are preserved.

Regeneration is an explicit maintainer action, from the repository root:

```powershell
python tests/compiler/patternmatch/regenerate.py --otp references/otp --escript 'C:/Program Files/Erlang OTP/erts-17.1/bin/escript.exe'
```

Use `--corpus integers` (or another directory name) to refresh only one corpus.
The generator verifies the pinned checkout and source manifests, generates the
modules and inputs, executes the selected OTP oracle, and writes the result
values with hashes, oracle version, reference revision, and adaptation details.
Review and commit those changes. Never regenerate expectations merely to make
a failing native comparison pass. Temporary generation directories stay under
the ignored `build/` directory.

`--check` regenerates into temporary storage and detects drift without changing
committed expectations. Configure with `-DERLANG_AOT_OTP_AUDITS=ON` to enable
separate live grammar, semantic-acceptance, provenance and fixture drift audits.
The default OFF leaves building and testing independent of OTP and its checkout.
Historical validation records retain their original live-oracle methodology.
