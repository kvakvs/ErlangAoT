# Local Erlang fragments

These source files are project-authored test inputs, with no upstream license
headers. Former copied OTP helpers were replaced by local implementations of
binding, aliasing, clause, guard, container, arithmetic, bitstring and record
behaviors. Some public helper names remain for compatibility with fixed calls.
The sources include deliberate semantic errors and unsupported features.

Each corpus has a `corpus.json` listing its fixed inputs and behavioral metadata.
Erlang modules and includes live here; call data, project manifests and retained
OTP observations live under [`../generated/`](../generated/README.md). The
fixture loader resolves both locations, checks their hashes and stages them in
the ignored build directory. Seeded stress inputs remain fixed for reproducible
comparisons. Ordinary tests require neither an OTP checkout nor an OTP runtime.

Format valid syntax with the ignored local formatter under `thirdparty/tools/erlfmt/`.
Then explicitly [refresh observations](../generated/README.md). OTP supplies
observations only. Original OTP source, generated OTP headers and audit outputs
must stay under ignored `references/` or `build/` directories.

See the [source audit](../../../../docs/otp-source-audit.md) for the migration and
the distinction between local source, observations and historical evidence.
