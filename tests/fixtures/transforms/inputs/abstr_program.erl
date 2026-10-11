-module(abstr_program).
-export([main/1]).

-record(point, {x = 0, y = 0}).

main(_) ->
    P = #point{x = 3},
    L = [X * 2 || X <- [1, 2, 3]],
    io:format("~p ~p~n", [P, L]),
    try
        throw(boom)
    catch
        throw:R -> io:format("caught ~p~n", [R])
    end,
    io:format("~p~n", [classify(<<7:4, 1:4>>)]),
    io:format("abstr_program: ok~n").

classify(<<High:4, _:4>>) when High > 5 -> high;
classify(_) -> low.
