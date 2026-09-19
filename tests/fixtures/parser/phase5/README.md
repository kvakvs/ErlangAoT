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
| 14 | `top_type`, `type`, `top_types`, type operators, `lift_unions`, `build_type`, `build_gen_type`, `type_tag` | `step14/types.erl`, `operators.erl`, `expanded.erl`; annotation/range/operator/float/list rejects |
| 14 | `fun_type`, `map_pair_types`, `field_types`, `binary_type`, `bin_base_type`, `bin_unit_type`, `build_bin_type` | All type fixtures; fun/map/record-field/binary-variable/unit/order rejects |
| 14 | `typed_attr_val`, `typed_record_spec`, `typed_record_fields`, `typed_exprs`, `typed_expr`, `build_typed_attribute` | Mixed tuple/native fields in `types.erl`; expanded defaults; parameter/head/typed-attribute rejects |

The native `parser_attributes` test covers raw `-doc({file, Path})` without opening
files: epp resolves documentation paths before `parse_erl_form`, so that boundary
is intentionally tested using the raw parser API. It also checks literal handle
ownership, stale generations, rollback, budgets and the public tree printer.
`parser_types` checks distinct type handles, foreign/stale child rejection, arena
rollback, destroyed source ownership, declaration structure and resource limits.
