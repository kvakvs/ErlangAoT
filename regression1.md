# OTP 29.1: record syntax in preprocessor conditions crashes compilation

## Reproduction

Verified with OTP 29.1 / ERTS 17.1 on macOS arm64, using OTP alone.

Save as `record_condition.erl`:

```erlang
-module(record_condition).
-export([value/0]).
-record(r, {a}).

-if(is_tuple(#r{a = 1})).
value() -> selected.
-else.
value() -> fallback.
-endif.
```

Compile with OTP 29.1:

```sh
/absolute/path/to/otp-29.1/bin/erlc record_condition.erl
```

## Symptoms

- Internal compiler error: `case_clause` on `{[], #Fun<...>}` in
  `erl_lint:is_gexpr/2`, called by `epp:assert_guard_expr/1`.
- The preprocessor process terminates; `erlc` exits with status 1 and produces
  no BEAM file.
- Also occurs in an evaluated `-elif`, behind `true orelse`, and without the
  record declaration. Replacing `#r{a = 1}` with `{r, 1}` compiles successfully.

## Suggested fix steps (separate implementation; unvalidated)

1. In OTP's `lib/stdlib/src/erl_lint.erl`, change the empty record context in
   `is_guard_expr/1` from `[]` to `#{}` to match the map expected by record
   validation.
2. Confirm unsupported record conditions produce a normal source diagnostic
   or the intended condition-failure behavior, without an uncaught exception.
3. Add regression tests for `-if`, evaluated `-elif`, `true orelse`, and a
   missing record declaration. Keep a tuple condition as a successful control;
   run the relevant OTP lint and preprocessor tests.
