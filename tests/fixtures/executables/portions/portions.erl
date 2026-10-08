-module(portions).
-export([main/1, count/1]).

% A busy process: N calls, then it ends.
count(0) -> ok;
count(N) -> count(N - 1).

% Run Work beside a busy process spawned just before it. The long builtin Work calls runs in portions, so the busy
% process gets time slices and ends before Work returns.
beside(Tag, Work) ->
    Busy = spawn(portions, count, [3000]),
    Result = Work(),
    io:format("~p ~p, busy process ended: ~p~n", [Tag, Result, not is_process_alive(Busy)]).

sum(List) -> sum(List, 0).

sum([], Acc) -> Acc;
sum([H | T], Acc) -> sum(T, Acc + H).

error_of(F) ->
    try F() of
        V -> {ok, V}
    catch
        error:R -> {error, R}
    end.

id(X) -> X.

main(_) ->
    Bytes = [X rem 256 || X <- lists:seq(1, 200000)],
    Binary = list_to_binary(Bytes),
    Left = lists:seq(1, 10000),
    Right = lists:reverse([X || X <- Left, X rem 2 =:= 0]),
    beside(append, fun() ->
        Appended = Bytes ++ [last],
        {length(Appended), lists:nth(200001, Appended)}
    end),
    beside(subtract, fun() ->
        Kept = Left -- Right,
        {length(Kept), sum(Kept)}
    end),
    beside(length, fun() -> length(Bytes) end),
    beside(binary_to_list, fun() -> sum(binary_to_list(Binary)) end),
    beside(list_to_binary, fun() -> byte_size(list_to_binary([Bytes, [Binary]])) end),
    beside(iolist_to_binary, fun() -> byte_size(iolist_to_binary([Binary | Bytes])) end),
    % Errors found late in a long walk are raised as before.
    io:format("~p~n", [
        [
            error_of(fun() -> (Bytes ++ id(tail)) ++ [x] end),
            error_of(fun() -> length(Bytes ++ id(tail)) end),
            error_of(fun() -> Left -- (Right ++ id(tail)) end),
            error_of(fun() -> (Left ++ id(tail)) -- Right end),
            error_of(fun() -> list_to_binary(Bytes ++ [256]) end),
            error_of(fun() -> iolist_to_binary(Bytes ++ id(tail)) end)
        ]
    ]).
