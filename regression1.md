# OTP 29.1: record construction in a preprocessor condition crashes the compiler

## Summary

Compiling a module containing `-if(is_tuple(#r{a = 1})).` causes an internal
compiler error in Erlang/OTP 29.1. The preprocessor process exits with a
`case_clause` exception in `erl_lint:is_gexpr/2` while validating the condition.
`erlc` exits with status 1 and does not produce a BEAM file.

The failure also occurs in an evaluated `-elif` condition and when the record
construction is behind `true orelse`. A record declaration is not required to
trigger it. This reproduction uses only OTP; ErlangAoT is not involved.

## Verified environment

- Erlang/OTP **29.1**, ERTS **17.1**, built from source.
- OTP Git tag: `OTP-29.1`.
- OTP Git commit: `751f87b703fe5948607d08e82599ce644b772e76` (clean source checkout).
- Host: macOS **26.6.2**, build **25G83**, **arm64**.
- Reproduced on 2026-09-18.

Runtime version output:

```text
Erlang/OTP 29 [erts-17.1] [source] [64-bit] [smp:8:8] [ds:8:8:10] [async-threads:1] [jit]
```

Only this version and platform were tested for this report. The first affected
release has not been identified; the filename does not imply a verified
regression from an earlier release.

## Minimal reproduction

Set `OTP_BIN` to the `bin` directory of an OTP 29.1 installation. In this
repository, the tested installation is `build/otp29-install/bin`.

Run these commands in a scratch directory:

```sh
OTP_BIN=/absolute/path/to/otp-29.1/bin

# Confirm the full release version; otp_release alone reports only "29".
"$OTP_BIN/erl" -noshell -eval '
    {ok, Version} = file:read_file(filename:join([
        code:root_dir(), "releases", erlang:system_info(otp_release), "OTP_VERSION"
    ])),
    io:format("~s~n~s", [erlang:system_info(system_version), Version]),
    halt().'

cat > record_condition.erl <<'EOF'
-module(record_condition).
-export([value/0]).
-record(r, {a}).

-if(is_tuple(#r{a = 1})).
value() -> selected.
-else.
value() -> fallback.
-endif.
EOF

"$OTP_BIN/erlc" record_condition.erl
printf 'exit status: %s\n' "$?"
```

## Actual result

The compiler prints the following error and exits with status **1**:

```text
*** Internal compiler error ***
exception exit: {{case_clause,{[],#Fun<erl_lint.56.79286395>}},
 [{erl_lint,is_gexpr,2,[{file,"erl_lint.erl"},{line,2979}]},
  {lists,all,2,[{file,"lists.erl"},{line,2312}]},
  {epp,assert_guard_expr,1,[{file,"epp.erl"},{line,1658}]},
  {epp,eval_if,2,[{file,"epp.erl"},{line,1644}]},
  {epp,scan_if,4,[{file,"epp.erl"},{line,1614}]}]}
  in function  epp:wait_epp_reply/2 (epp.erl:2271)
  in call from epp:parse_erl_form/1 (epp.erl:279)
  in call from epp:parse_file/1 (epp.erl:493)
  in call from epp:parse_file/1 (epp.erl:495)
  in call from epp:parse_file/2 (epp.erl:474)
  in call from compile:do_parse_module/2 (compile.erl:2011)
  in call from compile:parse_module/2 (compile.erl:1982)
  in call from compile:fold_comp/4 (compile.erl:1334)
```

The anonymous-function identifier may differ between builds. This is an Erlang
preprocessor-process failure surfaced as an internal compiler error, not an
observed native VM crash.

## Expected result

The preprocessor should handle the expression without an uncaught exception.
If record construction is unsupported in preprocessing conditions, it should
report a normal source diagnostic or follow the intended condition-failure
semantics. This report does not require record expansion during preprocessing
or assert that the example must select the `selected` branch.

## Additional verified cases

Apply each change independently to the module above:

| Change | Observed result on OTP 29.1 |
| --- | --- |
| Remove `-record(r, {a}).` | Same internal compiler error. |
| Use `-if(true orelse is_tuple(#r{a = 1})).` | Same `case_clause` during validation, despite short-circuiting. |
| Replace the initial `-if` with `-if(false).`, `value() -> first.`, then `-elif(is_tuple(#r{a = 1})).` | Same internal compiler error. |
| Use `-if(is_tuple({r, 1})).` | Compiles successfully, exit 0; unused-record warning. |
| Use `-if(lists:reverse([])).` | Normal `badly formed 'if'` diagnostic, exit 1; no internal compiler error. |

The tuple control indicates that the failure depends on record syntax, rather
than `is_tuple/1` alone. The invalid-guard control demonstrates the ordinary
diagnostic path.

## Isolated validator reproduction

The failure can also be reproduced without a source file or the compiler driver
by calling the validator used by `epp`:

```sh
"$OTP_BIN/erl" -noshell -eval '
    {ok, Tokens, _} = erl_scan:string("#r{a = 1}."),
    {ok, [Expr]} = erl_parse:parse_exprs(Tokens),
    try erl_lint:is_guard_expr(Expr) of
        Result -> io:format("~tp~n", [Result]), halt(0)
    catch
        Class:Reason:Stack ->
            io:format("~tp:~tp~n~tp~n", [Class, Reason, Stack]), halt(1)
    end.'
```

Observed: `error:{case_clause,{[],#Fun<erl_lint.56.79286395>}}`, with the first
stack frame at `erl_lint:is_gexpr/2`, line 2979. The command exits with status 1.
`erl_lint:is_guard_expr/1` is an internal, undocumented helper; the public-facing
reproduction is the `erlc` invocation above.

## Source-level diagnosis

The following observations refer to the pinned source commit above:

1. `erl_lint:is_guard_expr/1` initializes the validation context with
   `{[], fun({_,_}) -> false end}`. The first element is an empty **list**.
   [Source: erl_lint.erl, line 2959](https://github.com/erlang/otp/blob/751f87b703fe5948607d08e82599ce644b772e76/lib/stdlib/src/erl_lint.erl#L2959).
2. The record-construction clause of `is_gexpr/2` accepts a context whose first
   element is a **map**, or a zero-argument function returning the record
   definitions. Neither clause matches the empty list, causing the observed
   `case_clause`. The downstream `is_gexpr_fields/4` also uses `maps:find/2`.
   [Source: record validation, lines 2978–2985](https://github.com/erlang/otp/blob/751f87b703fe5948607d08e82599ce644b772e76/lib/stdlib/src/erl_lint.erl#L2978)
   and [field validation, line 3030](https://github.com/erlang/otp/blob/751f87b703fe5948607d08e82599ce644b772e76/lib/stdlib/src/erl_lint.erl#L3030).
3. `epp:eval_if/2` calls `assert_guard_expr/1` **before** entering the `try` that
   converts evaluation exceptions to `false`. `scan_if/4` catches `throw`
   exceptions, but this failure is an `error` exception. It therefore escapes
   and terminates the preprocessor process.
   [Source: epp.erl, lines 1613–1670](https://github.com/erlang/otp/blob/751f87b703fe5948607d08e82599ce644b772e76/lib/stdlib/src/epp.erl#L1613).

The list/map mismatch explains the reproduced crash. Initializing the record
context with an empty map is a candidate fix to investigate, but no OTP patch
was applied or validated for this report. A fix should include preprocessor
tests for `-if`, `-elif`, and short-circuit conditions, with the intended record
semantics made explicit.
