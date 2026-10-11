#!/usr/bin/env escript
%% Regenerate the term codec goldens with the host OTP (explicit run only):
%%   escript tests/compiler/transforms/terms_oracle.escript tests/fixtures/transforms/terms
%% samples.etf holds term_to_binary(Samples); samples.txt one `~tp.` term per sample.
-mode(compile).

main([Dir]) ->
    Samples = samples(),
    ok = file:write_file(filename:join(Dir, "samples.etf"), term_to_binary(Samples)),
    Text = [io_lib:format("~tp.~n", [Sample]) || Sample <- Samples],
    ok = file:write_file(filename:join(Dir, "samples.txt"), unicode:characters_to_binary(Text)),
    io:format("~p samples~n", [length(Samples)]).

samples() ->
    [
        ok,
        'Quoted',
        'with space',
        '',
        'case',
        'maybe',
        'a@b_1',
        'Ä',
        'λx',
        'tab\there',
        0,
        255,
        256,
        -1,
        2147483647,
        -2147483648,
        2147483648,
        18446744073709551616,
        -1267650600228229401496703205376,
        1 bsl 2100,
        0.0,
        -0.0,
        1.5,
        1.0e20,
        1.0e-5,
        123456.789,
        5.0e-324,
        1.7976931348623157e308,
        [],
        [1, 2, 3],
        "abc",
        "a\nb\"c\\",
        "λx",
        [1000, 2000],
        [a | b],
        [1, 2 | 3],
        [{x, y} | "tail"],
        lists:seq(1, 300),
        {},
        {a},
        list_to_tuple(lists:seq(1, 300)),
        #{},
        #{a => 1, 2 => [b], "k" => {}},
        <<>>,
        <<1, 2, 3>>,
        <<"abc">>,
        <<1:3>>,
        <<255, 7:4>>,
        {attribute, {1, 2}, module, m},
        {function, {3, 1}, f, 1, [
            {clause, {3, 1}, [{var, {3, 3}, 'X'}], [], [
                {op, {3, 11}, '+', {var, {3, 9}, 'X'}, {float, {3, 13}, 1.0}}
            ]}
        ]}
    ].
