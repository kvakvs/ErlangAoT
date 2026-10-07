-module(closures).
-export([main/1]).

% Higher-order helpers written with closures only.
map(_, []) -> [];
map(F, [H | T]) -> [F(H) | map(F, T)].

filter(_, []) ->
    [];
filter(P, [H | T]) ->
    case P(H) of
        true -> [H | filter(P, T)];
        false -> filter(P, T)
    end.

foldl(_, Acc, []) -> Acc;
foldl(F, Acc, [H | T]) -> foldl(F, F(H, Acc), T).

seq(N, N) -> [N];
seq(I, N) -> [I | seq(I + 1, N)].

compose(F, G) -> fun(X) -> F(G(X)) end.

adder(N) -> fun(X) -> X + N end.

counter(Start) ->
    fun
        (next) -> counter(Start + 1);
        (value) -> Start
    end.

captures() ->
    A = 10,
    B = {pair, 2},
    Add = fun(X) -> X + A end,
    Pair = fun() -> {A, B} end,
    erlang:display([Add(1), Pair()]),
    Inc = adder(1),
    Double = fun(X) -> X * 2 end,
    erlang:display([(compose(Inc, Double))(5), (compose(Double, Inc))(5)]),
    C = counter(0),
    erlang:display([(C(value)), ((C(next))(value)), (((C(next))(next))(value))]),
    erlang:display(map(adder(100), [1, 2, 3])),
    erlang:display(filter(fun(X) -> X rem 2 =:= 0 end, seq(1, 10))),
    erlang:display(foldl(fun(X, Acc) -> [X | Acc] end, [], [a, b, c])),
    Nested = fun(X) -> fun(Y) -> fun(Z) -> {X, Y, Z, A} end end end,
    erlang:display(((Nested(1))(2))(3)).

clauses() ->
    Limit = 5,
    Classify = fun
        (0) -> zero;
        (N) when N < 0 -> negative;
        (N) when N > Limit -> big;
        ({tag, V}) -> {tagged, V};
        (_) -> small
    end,
    erlang:display(map(Classify, [0, -3, 9, 2, {tag, x}])),
    Swap = fun({X, Y}) -> {Y, X} end,
    erlang:display(map(Swap, [{1, 2}, {a, b}])),
    erlang:display(catch_reason(fun() -> Swap(not_a_pair) end)),
    Sum = fun(X, Y) -> X + Y end,
    erlang:display(foldl(Sum, 0, seq(1, 100))).

comparisons() ->
    F1 = adder(1),
    F2 = adder(1),
    F3 = adder(2),
    F4 = adder(1.0),
    erlang:display([F1 =:= F2, F1 == F2, F1 =:= F3, F1 < F3, F3 > F1, F1 == F4, F1 =:= F4]),
    erlang:display([is_function(F1), is_function(F1, 1), is_function(F1, 0), F1 < fun lists:map/2]),
    Kind = fun
        (V) when is_function(V, 1) -> one;
        (V) when is_function(V) -> other;
        (_) -> none
    end,
    erlang:display(map(Kind, [F1, fun() -> ok end, 3])).

% Closures created in loops survive the creator's return and collections.
make(0, Acc) ->
    Acc;
make(N, Acc) ->
    Tuple = {N, [N, N + 1], <<N:32>>},
    make(N - 1, [fun(K) -> {element(1, Tuple) + K, Tuple} end | Acc]).

loops() ->
    Funs = make(20000, []),
    Total = foldl(fun(F, Acc) -> element(1, F(1)) + Acc end, 0, Funs),
    erlang:display(Total),
    erlang:display(element(2, (lists_last(Funs))(0))),
    Squares = [fun() -> X * X end || X <- seq(1, 5)],
    erlang:display(map(fun(F) -> F() end, Squares)).

lists_last([X]) -> X;
lists_last([_ | T]) -> lists_last(T).

% A closure calling itself through an argument, in tail position: constant stack.
loop(_, 0, Acc) -> Acc;
loop(Step, N, Acc) -> Step(Step, N, Acc).

tails() ->
    Step = fun(Self, N, Acc) -> loop(Self, N - 1, Acc + N) end,
    erlang:display(loop(Step, 100000, 0)).

errors() ->
    Add = adder(1),
    erlang:display(catch_reason(fun() -> Add(1, 2) end)),
    erlang:display(catch_reason(fun() -> apply_one(fun() -> ok end, x) end)),
    erlang:display(catch_reason(fun() -> apply_one(fun(1) -> one end, 2) end)),
    erlang:display(catch_reason(fun() -> Add(atom) end)).

apply_one(F, X) -> F(X).

catch_reason(F) ->
    try F() of
        Value -> {ok, Value}
    catch
        error:{badarity, {Fun, Args}} -> {badarity, is_function(Fun), Args};
        Class:Reason -> {Class, Reason}
    end.

peers() ->
    P = clospeer:make(3),
    erlang:display([P(x), P(y), (P({add, 10}))(y), clospeer:call(P, x)]),
    erlang:display(clospeer:scopes({a, 5})),
    erlang:display(clospeer:scopes(7)).

main([]) ->
    peers(),
    captures(),
    clauses(),
    comparisons(),
    loops(),
    tails(),
    errors();
main(["clause"]) ->
    apply_one(fun(ok) -> ok end, nope).
