# Native records

Decision of plan 11 step 31A (2026-10-07). It scopes the OTP 29 record forms
beyond ordinary tuple records and fixes their representation, operations and
errors. Steps 31B–31E implement it; until then the forms keep the
`[heap expressions] notimpl` diagnostic. Tuple records are in
[terms](terms.md#records).

OTP 29 marks native records experimental. Facts below come from the pinned
`maint-29` sources (`erl_lint`, `erl_expand_records`, `v3_core`,
`beam_core_to_ssa`, `erts/emulator/beam/erl_record.c`, `utils.c`,
`erl_printf_term.c`) and live probes on OTP 29.1.1.

## Scope

All three forms beyond tuple records are selected. They share one
representation, so they need no separate representation decision:

| Form | Syntax | Step |
| --- | --- | --- |
| Native record, local | `-record #r{...}.`, `#r{...}`, `X#r.f`, `X#r{...}`, patterns, `is_record/1,2,3` | 31C |
| Qualified and imported | `#m:r{...}`, `X#m:r.f`, `X#m:r{...}`, patterns; `-export_record`, `-import_record` | 31D |
| Inferred (anonymous) | `X#_.f`, `X#_{...}`, pattern `#_{...}` | 31E |

Not selected: the `records` reflection module (a library, with step 39's
library work), `term_to_binary` (`RECORD_EXT`) and hot code upgrade (D01).
Calls to `records:*` stay `unknown module` diagnostics.

## Representation

- **Descriptor.** Each native record definition compiles to one immutable
  `abi::v1::RecordDescriptor` beside its module descriptor: the defining
  `ModuleDescriptor`, atom slots for the module and record names, the export
  flag (`-export_record` at compile time) and the field-name atom slots in
  definition order. Atom slots resolve through the module's registered atoms,
  as `FrameDescriptor` names do. Programs are static (no unloading), so a
  descriptor lives as long as the program; another module of the batch refers
  to it by symbol. Defaults are compiled into each construction site, never
  stored.
- **Instance.** Heap cell of kind `native_record`: header (count `1 + n`), an
  untraced descriptor word, then `n` traced values in definition order. A
  record with no fields is a two-word cell.
- The descriptor captures module, name, export flag and field list at
  construction, as OTP's captured definition does. Every operation after
  construction reads the instance's descriptor, never the code that runs it.
- Copying, collection, walking and verification treat the cell as a tuple
  whose first payload word is untraced. Admission requires a descriptor
  registered by a module of the same runtime.

## Operations

`M` is the current module. All errors have class `error`; badrecord payloads
are the offending value except failed external construction.

| Operation | Accepts | Failure |
| --- | --- | --- |
| Local construction `#r{...}` | Always (definition is local) | Unknown field or missing value are compile errors |
| External construction `#m:r{...}` (also imported `#r{...}`, also `#M:r`) | `m:r` defined in the batch and exported | `{badrecord, {m, r}}`; then `{badfield, {{m, r}, F}}`, `{novalue, {{m, r}, F}}` (badfield wins) |
| Local access `X#r.f` | Native record named `r` (module and export flag **not** checked, as OTP's runtime) | `{badrecord, X}`; field missing `{badfield, {{Mod, r}, f}}` |
| External access `X#m:r.f` | Module `m`, name `r`, exported | `{badrecord, X}`; `{badfield, ...}` |
| Anonymous access `X#_.f` | Any native record (export **not** checked, as OTP's runtime) | `{badrecord, X}`; `{badfield, ...}` |
| Local update `X#r{...}` | Module `M`, name `r` | `{badrecord, X}`; unknown field `{badfield, ...}` |
| External update `X#m:r{...}` | Exported, module `m`, name `r` | same |
| Anonymous update `X#_{...}` | Exported or module `M` | same |
| Local pattern `#r{...}` | Module `M`, name `r`, every listed field present | no match |
| External pattern `#m:r{...}` | Module `m`, name `r`; exported when a field is listed | no match |
| Anonymous pattern `#_{...}` | Any native record; exported or module `M` when a field is listed | no match |

- Update evaluates new values in source order, then the record (as tuple
  updates); construction evaluates fields in definition order with defaults
  for omitted fields. Empty updates still check.
- Guards: only field access (a failure fails the guard) and `is_record`.
  Construction in a guard is `creating a record in a guard is only supported
  for tuple records`; update is `illegal guard expression`.
- `is_record(X)` is true only for native records. `is_record(X, r)` with a
  local native `r` tests module `M` and name `r`; with `r` imported from `m` it
  tests module `m`. `is_record(X, m, r)` with atoms tests module
  and name; with an integer it is the tuple test.
- Native records are not tuples: `is_tuple` is false; element and size
  services reject them.

## Compile-time rules

Messages follow `erl_lint`:

- Defaults must be literal after constant folding (numbers, atoms, strings,
  `[]`, conses/tuples/maps of those, string-only binaries):
  `illegal default value for field a in native record r`.
- Local construction without a value for a field without default:
  `field a is not initialized in native record r`; unknown field:
  `field zz undefined in record r`. Unknown fields in access, update and
  patterns are accepted (OTP only warns); they fail at run time.
- `#r.a` index, `record_info/2`, `_ = V` initializers and typed field
  refinements are tuple-record only, with OTP's messages.
- A name is either a tuple record, a native record or an imported record;
  conflicts and bad `-export_record`/`-import_record` forms are errors.
  `-export_record` must precede function definitions.
- External forms naming a module outside the batch, an undefined or a
  non-exported record compile and fail at run time like OTP.

## Printing and order

- `erlang:display/1` prints `#m:r{a=1,b=2}`: no spaces, fields in definition
  order. OTP prints fields in atom-table index order, which depends on atom
  creation order and is not reproducible; this is a recorded difference, as
  for map keys.
- Term order: tuple < native record < map. Records compare by module, name,
  export flag (false first), field count, field names in definition order,
  then values in definition order. `=:=` additionally requires the same field
  order; `==` compares values numerically (OTP's compiler folds some `==` on
  records to `=:=`; the runtime rule is followed).

## Differences kept

| OTP | ErlangAoT |
| --- | --- |
| `display` field order follows atom indexes | Definition order |
| External construction of a module that is not loaded fails until it is loaded | Every batch module is loaded at startup; a module outside the batch always fails |
| Compiler may fold `==` between records to `=:=` | Runtime `==` |
| Unknown fields in access/update/patterns, own-module qualified construction without values, unused or header-defined records: warnings | No warnings |
