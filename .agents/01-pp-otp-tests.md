# OTP preprocessor test inventory

Inspected 2026-09-18. Local checkout: `references/otp/`, ignored by the parent
repository. Upstream: <https://github.com/erlang/otp>. Shallow checkout of
`OTP-29.1`, commit `751f87b703fe5948607d08e82599ce644b772e76`.
The tag is annotated; its tag-object hash differs from the commit above.

Recreate from the project root with:

```sh
git clone --depth 1 --branch OTP-29.1 https://github.com/erlang/otp.git references/otp
git -C references/otp rev-parse HEAD
```

This is a source reference, not a build dependency or Git submodule. Source and
tests were inspected; OTP was not built and its suites were not executed.

## Main preprocessor suite

The compiler uses epp from **stdlib**. Start with
[`lib/stdlib/test/epp_SUITE.erl`](../references/otp/lib/stdlib/test/epp_SUITE.erl)
and [`epp_SUITE_data/`](../references/otp/lib/stdlib/test/epp_SUITE_data/).
Many fixtures are Erlang source strings embedded in the suite, accompanied by
expected errors, warnings, or execution results; searching only the data directory
misses much of the coverage.

| Area / plan steps | Cases to inspect in `epp_SUITE.erl` |
| --- | --- |
| Basic definitions and predefinitions / 4 | `predef_mac`, `upcase_mac_1`, `upcase_mac_2`, `variable_1`, `otp_8130` |
| Recursion and expansion / 4–5 | `rec_1`, `not_circular`, `otp_11728`; data files `mac.erl`, `mac2.erl`, `mac3.erl` |
| Overloads, argument handling / 5 | `overload_mac`, `otp_8388`, `otp_8130`, `fun_type_arg` |
| Stringification / 6 | `otp_7702`, `otp_8130`, `stringify`, `not_circular` |
| Conditional structure / 7 | `ifdef` (a helper called by `otp_8130`), `otp_16824` malformed-directive cases |
| Conditional expressions / 10 | `test_if` |
| Includes / 8 | `include_local`, `otp_8130`, `otp_8911`, `otp_10820` |
| File/line context / 9 | `otp_5362`, `otp_7702`, `file_macro`, `source_name`, `deterministic_include`, `nondeterministic_include` |
| Function context / 9 | `function_macro` |
| Encoding / 2 | `encoding`, `otp_10302`, `otp_14285` |
| User diagnostics / 12 | `test_error`, `test_warning` |
| Scan API and feature-sensitive argument collection / 1, 5, 11 | `scan_file`, `gh_8268` |

The implementation counterpart is
[`lib/stdlib/src/epp.erl`](../references/otp/lib/stdlib/src/epp.erl).
Case names are more durable navigation anchors than line numbers.

## Feature controls and compiler integration

- [`erts/test/erlc_SUITE.erl`](../references/otp/erts/test/erlc_SUITE.erl):
  `features_directives`, `features_macros`, `features_include`, `features_disable`,
  `features_all`, `features_erlc_unknown`, and `features_atom_warnings` cover
  directive placement, query macros, inclusion, configuration, and keyword effects.
  Follow `feature_tests/0` for the complete group. Runtime/loading cases are wider
  than the current preprocessor task.
- [`erts/test/erlc_SUITE_data/src/`](../references/otp/erts/test/erlc_SUITE_data/src/):
  `f_macros.erl`, `f_directives.erl`, `f_disable.erl`, `f_include_1.erl` through
  `f_include_3.erl`, `f_include_exp2.erl`, and associated headers provide explicit
  source fixtures. These feature cases use `OTP_TEST_FEATURES=true`, enabled by
  the suite, and synthetic features such as `experimental_ftr_1`. Do not copy
  those names into the production feature catalog.
- [`lib/compiler/test/compile_SUITE.erl`](../references/otp/lib/compiler/test/compile_SUITE.erl):
  `cond_and_ifdef` combines compiler definitions and include options, compiles
  `compile_SUITE_data/simple.erl`, and checks its result. `makedep` exercises
  dependency output; `listings` and `deterministic_include` provide adjacent
  output/source-name coverage. Dependency/listing formats remain separate work.
- [`lib/stdlib/test/erl_scan_SUITE.erl`](../references/otp/lib/stdlib/test/erl_scan_SUITE.erl):
  scanner tests supplement epp coverage, especially `sigil_string`,
  `triple_quoted_string`, and the lexical/Unicode cases under `otp_7810`.
  Consult [`erl_scan.erl`](../references/otp/lib/stdlib/src/erl_scan.erl) and
  [`erl_features.erl`](../references/otp/lib/stdlib/src/erl_features.erl) alongside them.

## Specific lessons for our plan

1. `include_local` asserts that a nested header finds its sibling header even
   when an include search directory contains a conflicting name. Preserve this
   fixture when implementing the include stack and resolver.
2. `fun_type_arg` tests fun *types* inside macro arguments, as well as fun
   expressions, guards containing commas, map updates, and extra parentheses.
   Counting every `fun` as an opening construct requiring `end` is incorrect.
3. `test_if` includes a non-boolean condition (`42`) that selects the alternative
   branch, and an arithmetic failure whose body is skipped. Do not use C++
   truthiness or turn every evaluation failure into a preprocessing diagnostic.
4. `otp_8130` accepts an object definition alongside parameterized definitions;
   it also covers duplicate parameter errors and non-parameter `??B` behavior.
   Use these cases before inventing stricter macro validation.
5. `function_macro` includes macro-generated headers, contextual expansion inside
   arguments, and illegal uses of function-context macros. It is the starting
   fixture set for the limited header recognizer.
6. `eval_tests/3` reruns embedded cases with `maybe_expr` enabled, testing the
   change in classification of `else`. Preserve that interaction in our harness.

## Reuse and execution approach

Begin with selected inputs and expected preprocessing outcomes, retaining the
upstream case name and pinned commit in our fixture metadata. Preserve upstream
notices for copied material. Keep scanner, epp, parser, compiler, and runtime
assertions distinguishable: a parser failure after preprocessing is not itself
an epp failure, and compile-and-run fixtures need an adapted preprocessing oracle.

The suite's `check/2`, `compile/2`, and `run/2` helpers respectively exercise
preprocessing/parsing errors, compilation diagnostics, and compiled behavior.
Use `epp:scan_erl_form/1` to derive expectations for our token-level tests; use
parsing and compilation as additional checks when their stages are available.

For an eventual upstream run, follow
[`HOWTO/TESTING.md`](../references/otp/HOWTO/TESTING.md) from this checkout.
After its OTP build/test setup, the documented make interface permits:

```sh
make stdlib_test ARGS="-suite epp_SUITE"
make stdlib_test ARGS="-suite erl_scan_SUITE"
make compiler_test ARGS="-suite compile_SUITE -case cond_and_ifdef"
```

Run these from the OTP root with its matching built release and test environment.
They are not commands for a freshly cloned, unconfigured tree. Suites use OTP's
`ts_install_cth` hook and test infrastructure; do not assume bare `ct_run` or an
unrelated installed Erlang version reproduces the baseline.
