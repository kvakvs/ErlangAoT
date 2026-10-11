-module(ms).
-export([main/1]).
-compile({parse_transform, ms_transform}).

main(_) ->
    Spec = ets:fun2ms(fun({A, B}) when A > 1 -> B end),
    io:format("~p~n", [Spec]),
    io:format("ms: ok~n").
