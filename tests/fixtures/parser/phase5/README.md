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
| 15 | `type_spec`, `spec_fun`, `type_sigs`, `type_sig`, `type_guards`, `type_guard`, `build_type_spec`, `build_constraint`, `build_compat_constraint` | `step15/specifications.erl`, `contextual.erl`, `expanded.erl`, malformed constraint/overload/name/result cases |
| 15 | Contextual `record` helper production, exceptional builder shapes, parser/lint boundary | `record_helper.builder-reject`, `record_extra.builder-reject`, `any_first.builder-reject`, `lint_only.erl` |

The native `parser_attributes` test covers raw `-doc({file, Path})` without opening
files: epp resolves documentation paths before `parse_erl_form`, so that boundary
is intentionally tested using the raw parser API. It also checks literal handle
ownership, stale generations, rollback, budgets and the public tree printer.
`parser_types` checks distinct type handles, foreign/stale child rejection, arena
rollback, destroyed source ownership, declaration structure and resource limits.

`coverage.tsv` links every historical Phase V grammar row to concrete fixtures;
the runner verifies those paths. The pinned historical matrices/checksums remain
unchanged. `parser_specifications` covers products, styles, qualification, recovery,
limits, builder checks and public printing.

OTP's `record type_spec` action calls `build_type_spec` with an unsupported helper
kind, and a first `(...)` signature fails the builder's product match. These raise
exceptions rather than yielding abstract forms. `.builder-reject` cases run raw
`erl_parse:parse_form` and explicitly verify `function_clause`/`badmatch`; ErlangAoT
reports a normal parser diagnostic and continues. They are not counted as accepted
OTP syntax or silently treated as ordinary OTP error tuples.
