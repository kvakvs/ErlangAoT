# Phase IV parser fixtures

Authored from the pinned OTP 29.1 grammar; no upstream fixture code was copied.
New `.erl.ast`/`.erl.lint` records are generated with the shared oracle adapter on
the installed Homebrew OTP 29.0.5. Native golden comparisons run offline; live
comparisons use CMake's selected OTP >=29. The grammar target remains OTP 29.1.

Step 10 covers begin, case, guard-only if, all three receive alternatives, nested
delimiters, macro arguments, permissive branch candidates, and rejected empty or
incomplete blocks. `control.erl.lint` records parse success with deferred lint errors.

Step 11 covers local/dynamic remote references, anonymous/recursive funs, every try
tail combination, catch defaults and stacktrace rules, maybe/conditional matches,
else clauses, and disabled maybe keywords. Included macro bodies cover pipeline
interactions. New syntax projections normalize catch omissions only for OTP parity.

Step 12 `generators.erl` crosses every output kind with every generator kind/strictness.
`structure.erl` exercises multiple list/map templates, mixed and filter-containing zip
groups, nesting, arbitrary candidates and binary expr_max boundaries. Assignment
fixtures preserve both feature states: enabling compr_assign permits lint, disabling
it rejects the same syntax. The lint adapter forwards epp feature metadata. Rejections
cover malformed qualifiers/templates, restricted pattern roots, invalid binary
heads/templates, quoted arrows and map-update comprehensions.

| Grammar coverage | Positive fixture | Negative coverage |
| --- | --- | --- |
| begin/case/if/receive alternatives | step10/control | step10 empty/missing delimiters, restricted head |
| local/remote, anonymous/named fun | step11/funs_try_maybe, alternatives | local/remote arity, mixed/mismatched/empty heads |
| try with/without of, catch, after | step11/funs_try_maybe, alternatives | bare/of-only try, empty catch, reason/stacktrace restrictions |
| maybe body, conditional matches, else, feature keywords | step11/funs_try_maybe, alternatives, disabled | empty/chained/out-of-context matches and disabled keyword |
| list/map/binary × ordinary/strict list/map/binary generator | step12/generators | wrong generator heads/arrows |
| single/multiple list/map templates; binary expr_max | step12/structure, generators | empty/improper/trailing templates, binary size/type/unary/call |
| sequential/zip, mixed generators/filters, nesting | step12/structure | missing/parenthesized zip and trailing qualifier |
| compr_assign match filters and feature state | step12/assign_enable, assign_disable | same syntax parses in both; only enabled fixture passes lint |
