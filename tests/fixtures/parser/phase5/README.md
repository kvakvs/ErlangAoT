# Attribute and type compatibility

Fixtures use the pinned OTP 29.1 `erl_parse.yrl` grammar and builder actions.
Golden projections were recorded with the locally installed OTP 29.0.5; CMake
replays them against its discovered OTP 29+ installation. These are private test
records, not a stage interchange format.

| Step | Grammar / builders | Fixtures |
| --- | --- | --- |
| 13 | `attribute`, `attr_val`, `build_attribute`, `attribute_farity`, `var_list`, `farity_list`, `native_record_name_list` | `step13/attributes.erl`, `envelopes.erl`, `bad_{export,import,module,term,call,key,map}.reject` |
| 13 | `record_spec`, `build_record`, `record_tuple`, `record_fields` | Both valid fixtures; `bad_field.reject`, `bad_native.reject` |
| 13 | `normalise`, documentation metadata, literal bits and external fun data | Both valid fixtures; `bad_doc*.reject`, `bad_binary.reject` |

The native `parser_attributes` test covers raw `-doc({file, Path})` without opening
files: epp resolves documentation paths before `parse_erl_form`, so that boundary
is intentionally tested using the raw parser API. It also checks literal handle
ownership, stale generations, rollback, budgets and the public tree printer.
