-module(library).
-export([main/1]).

%% The project-owned lists and maps subsets, compiled from library sources
%% because this module calls them: every function with valid, boundary and
%% invalid arguments.
main([]) ->
    [erlang:display(outcome(M, F, A)) || {M, F, A} <- id(calls())],
    order(),
    values(),
    large().

id(X) -> X.

%% One call per line: {ok, Value} or the class and reason it raised.
calls() ->
    Double = fun(X) -> X * 2 end,
    Odd = fun(X) -> X rem 2 =:= 1 end,
    Cons = fun(X, Acc) -> [X | Acc] end,
    Pair = fun(K, V, Acc) -> [{K, V} | Acc] end,
    Map = #{1 => one, 2 => two, 3 => three},
    [
        {lists, reverse, [[]]},
        {lists, reverse, [[a]]},
        {lists, reverse, [[a, b]]},
        {lists, reverse, [[a, b, c, d, e]]},
        {lists, reverse, [[a | b]]},
        {lists, reverse, [[a, b | c]]},
        {lists, reverse, [x]},
        {lists, reverse, [[a, b], [c]]},
        {lists, reverse, [[], tail]},
        {lists, reverse, [[a | b], []]},
        {lists, map, [Double, [1, 2, 3]]},
        {lists, map, [Double, []]},
        {lists, map, [Double, [1 | 2]]},
        {lists, map, [Double, x]},
        {lists, map, [x, [1]]},
        {lists, map, [Double, [a]]},
        {lists, foldl, [Cons, [], [1, 2, 3]]},
        {lists, foldl, [Cons, acc, []]},
        {lists, foldl, [Cons, [], x]},
        {lists, foldl, [x, [], [1]]},
        {lists, foldr, [Cons, [], [1, 2, 3]]},
        {lists, foldr, [Cons, acc, []]},
        {lists, foldr, [Cons, [], [1 | 2]]},
        {lists, filter, [Odd, [1, 2, 3, 4, 5]]},
        {lists, filter, [Odd, []]},
        {lists, filter, [Odd, x]},
        {lists, filter, [Double, [1]]},
        {lists, filter, [x, [1]]},
        {lists, member, [b, [a, b, c]]},
        {lists, member, [d, [a, b, c]]},
        {lists, member, [1, [1.0]]},
        {lists, member, [a, [a | b]]},
        {lists, member, [z, [a | b]]},
        {lists, member, [a, x]},
        {lists, keyfind, [b, 1, [{a, 1}, {b, 2}, {b, 3}]]},
        {lists, keyfind, [2, 2, [{a, 1}, {b, 2.0}]]},
        {lists, keyfind, [c, 1, [{a, 1}, x, {}]]},
        {lists, keyfind, [a, 3, [{a, 1}]]},
        {lists, keyfind, [a, 0, [{a, 1}]]},
        {lists, keyfind, [a, x, [{a, 1}]]},
        {lists, keyfind, [z, 1, [{a, 1} | b]]},
        {lists, keyfind, [z, 1, x]},
        {lists, sort, [[3, 1, 2]]},
        {lists, sort, [[]]},
        {lists, sort, [[x]]},
        {lists, sort, [[b, 2, {}, a, [], 1.5, "s", <<"b">>]]},
        {lists, sort, [[5, 4, 3, 2, 1, 1, 2, 3]]},
        {lists, sort, [x]},
        {lists, seq, [1, 5]},
        {lists, seq, [1, 0]},
        {lists, seq, [5, 5]},
        {lists, seq, [-2, 2]},
        {lists, seq, [5, 1]},
        {lists, seq, [1, a]},
        {lists, seq, [1.0, 2]},
        {lists, seq, [1, 10, 3]},
        {lists, seq, [10, 1, -3]},
        {lists, seq, [1, 1, 0]},
        {lists, seq, [1, 0, 1]},
        {lists, seq, [1, 10, 0]},
        {lists, seq, [1, 10, -1]},
        {lists, seq, [1, 2, a]},
        {lists, nth, [1, [a, b]]},
        {lists, nth, [2, [a, b]]},
        {lists, nth, [3, [a, b]]},
        {lists, nth, [0, [a]]},
        {lists, nth, [a, [a]]},
        {lists, append, [[[1], [2, 3], [], [4]]]},
        {lists, append, [[]]},
        {lists, append, [[[1], x]]},
        {lists, append, [[x, [1]]]},
        {lists, append, [[1], [2]]},
        {lists, append, [[1], x]},
        {lists, append, [x, [1]]},
        {maps, get, [1, Map]},
        {maps, get, [4, Map]},
        {maps, get, [1, x]},
        {maps, put, [4, four, Map]},
        {maps, put, [1, uno, Map]},
        {maps, put, [1, uno, x]},
        {maps, find, [2, Map]},
        {maps, find, [4, Map]},
        {maps, find, [2.0, Map]},
        {maps, find, [1, x]},
        {maps, keys, [Map]},
        {maps, keys, [#{}]},
        {maps, keys, [x]},
        {maps, values, [Map]},
        {maps, values, [x]},
        {maps, fold, [Pair, [], Map]},
        {maps, fold, [Pair, acc, #{}]},
        {maps, fold, [Pair, [], x]},
        {maps, fold, [x, [], Map]},
        {maps, from_list, [[{1, a}, {2, b}, {1, c}]]},
        {maps, from_list, [[]]},
        {maps, from_list, [[{1, a} | x]]},
        {maps, from_list, [[a]]},
        {maps, from_list, [x]},
        {maps, to_list, [Map]},
        {maps, to_list, [#{}]},
        {maps, to_list, [x]}
    ].

%% The value of M:F(Args), or the class and reason it raised.
outcome(M, F, A) ->
    try apply(M, F, A) of
        Value -> {ok, Value}
    catch
        Class:Reason -> {Class, Reason}
    end.

%% The order in which funs are applied.
order() ->
    Show = fun(X) ->
        erlang:display({visit, X}),
        X
    end,
    lists:map(Show, [1, 2]),
    lists:foldl(fun(X, _) -> Show(X) end, 0, [3, 4]),
    lists:foldr(fun(X, _) -> Show(X) end, 0, [5, 6]),
    lists:filter(fun(X) -> Show(X) > 7 end, [7, 8]),
    maps:fold(fun(K, _, _) -> Show(K) end, 0, #{10 => a, 9 => b}).

%% Library functions as values, through apply/3 and dynamic calls.
values() ->
    M = id(lists),
    erlang:display([
        (fun lists:reverse/1)([1, 2]),
        apply(lists, seq, [1, 3]),
        M:nth(2, [a, b]),
        lists:map(fun lists:reverse/1, [[1, 2], [3]]),
        maps:to_list(maps:from_list([{K, K * K} || K <- lists:seq(1, 32)])) =:=
            [{K, K * K} || K <- lists:seq(1, 32)]
    ]).

%% 10,000-element lists.
large() ->
    L = lists:seq(1, 10000),
    R = lists:reverse(L),
    erlang:display([
        lists:sort(R) =:= L,
        lists:foldl(fun erlang:'+'/2, 0, L),
        length(lists:filter(fun(X) -> X rem 3 =:= 0 end, L)),
        lists:nth(10000, L),
        lists:member(10000, R),
        lists:keyfind(9999, 1, [{X} || X <- L]),
        lists:foldr(fun(X, Acc) -> X + Acc end, 0, L),
        length(lists:append([L, R])),
        lists:seq(1, 10000, 2500)
    ]).
