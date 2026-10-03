%% Exercises the AVL library with pseudo-random and sorted keys.
-module(avltree).
-export([main/1]).

%% Builds, queries, prunes and transforms trees, printing each result.
main(_Args) ->
    Keys = random_keys(40, 2026),
    Tree = lists:foldl(fun(K, T) -> avl:insert(K, K * K, T) end, avl:new(), Keys),
    io:format("fields: ~w~n", [avl:fields()]),
    io:format("keys: ~w~n", [Keys]),
    describe(random, Tree),
    io:format("in order: ~w~n", [[K || {K, _} <- avl:to_list(Tree)]]),
    Probes = [hd(Keys), lists:nth(10, Keys), 7, 1000],
    io:format("lookups: ~w~n", [[{K, avl:lookup(K, Tree)} || K <- Probes]]),
    Pruned = lists:foldl(fun avl:delete/2, Tree, [K || K <- Keys, K rem 3 =:= 0]),
    describe(pruned, Pruned),
    io:format("pruned: ~w~n", [avl:to_list(Pruned)]),
    Doubled = avl:map(fun(V) -> V * 2 end, Pruned),
    Sum = avl:fold(fun(_, V, Acc) -> V + Acc end, 0, Doubled),
    io:format("doubled sum: ~b, count via apply: ~b~n", [Sum, apply(avl, count, [Doubled])]),
    Sorted = lists:foldl(fun(K, T) -> avl:insert(K, [], T) end, avl:new(), lists:seq(1, 1000)),
    describe(sorted, Sorted),
    Emptied = lists:foldl(fun avl:delete/2, Sorted, lists:seq(1000, 1, -1)),
    describe(emptied, Emptied).

%% Prints size, height and the invariant check of a tree.
describe(Label, Tree) ->
    io:format("~w: count ~b, height ~b, check ~w~n", [
        Label, avl:count(Tree), avl:height(Tree), avl:check(Tree)
    ]).

%% Generates Count keys in 0..99 with a 31-bit linear congruential generator.
random_keys(Count, Seed) ->
    Next = fun
        Step(0, _, Acc) ->
            lists:reverse(Acc);
        Step(N, State, Acc) ->
            State1 = (State * 1103515245 + 12345) rem 2147483648,
            Step(N - 1, State1, [State1 rem 100 | Acc])
    end,
    Next(Count, Seed, []).
