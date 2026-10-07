-module(named_funs).
-export([main/1]).
-compile(nowarn_shadow_vars).

%% One frame per step would overflow the authored runs' 4096-byte stack.
-define(STEPS, 10000).

main([]) ->
    recursion(),
    identity(),
    scopes(),
    higher_order(),
    errors();
main(["loop"]) ->
    Loop = fun
        L(0, Acc) -> Acc;
        L(N, Acc) -> L(N - 1, Acc + 1)
    end,
    erlang:display({loop, Loop(id(?STEPS), 0)});
main(["captured_loop"]) ->
    Step = id(3),
    Loop = fun
        L(0, Acc) -> Acc;
        L(N, Acc) -> L(N - 1, Acc + Step)
    end,
    erlang:display({captured_loop, Loop(id(?STEPS), 0)});
main(["clause"]) ->
    Down = fun D(N) when N > 0 -> D(N - 1) end,
    Down(id(2)).

id(X) -> X.

two(F) -> F(1, 2).

recursion() ->
    Fact = fun
        F(0) -> 1;
        F(N) -> N * F(N - 1)
    end,
    erlang:display([Fact(0), Fact(5), Fact(25)]),
    Seq = fun
        S(0) -> [];
        S(N) when N > 0 -> [N | S(N - 1)]
    end,
    Len = fun
        L([]) -> 0;
        L([_ | T]) -> 1 + L(T)
    end,
    Long = Seq(id(20000)),
    erlang:display({Len(Long), hd(Long), lists_last(Long)}),
    Fib = fun
        Fib(N) when N < 2 -> N;
        Fib(N) -> Fib(N - 1) + Fib(N - 2)
    end,
    erlang:display(Fib(20)).

lists_last([X]) -> X;
lists_last([_ | T]) -> lists_last(T).

identity() ->
    H = fun Self(_) -> Self end,
    erlang:display([H(1) =:= H, H(1) == H, is_function(H(x), 1), H(1) =:= fun Self(_) -> Self end]),
    Base = id(10),
    Make = fun(B) -> fun Self(get) -> {B, Self} end end,
    One = Make(Base),
    {10, Again} = One(get),
    erlang:display([Again =:= One, Again =:= Make(Base), Again =:= Make(11)]).

scopes() ->
    F = 1,
    G = fun
        F(0) -> done;
        F(N) -> F(N - 1)
    end,
    erlang:display({F, G(3)}),
    Arg = fun
        F(X) when X > 0 -> F(X - 1);
        F(F) -> {F, F}
    end,
    erlang:display(Arg(5)),
    Base = id(100),
    Sum = fun
        S([]) -> Base;
        S([X | T]) -> X + S(T)
    end,
    erlang:display(Sum([1, 2, 3])),
    Outer = fun
        O(0) ->
            [];
        O(N) ->
            Inner = fun() -> O(N - 1) end,
            [N | Inner()]
    end,
    erlang:display(Outer(4)),
    Guarded = fun
        Gd(X) when is_function(Gd, 1), X > 0 -> Gd(X - 1);
        Gd(X) -> {bottom, X}
    end,
    erlang:display(Guarded(3)).

higher_order() ->
    Map = fun
        M(_, []) -> [];
        M(Fun, [H | T]) -> [Fun(H) | M(Fun, T)]
    end,
    erlang:display(Map(fun(X) -> X * X end, [1, 2, 3, 4])),
    Twice = fun(Fun, X) -> Fun(Fun(X)) end,
    Halve = fun
        Hv(X) when X > 100 -> Hv(X div 2);
        Hv(X) -> X
    end,
    erlang:display(Twice(Halve, 1000)),
    Pair = {Map, Halve},
    {M2, _} = Pair,
    erlang:display(M2(fun(X) -> -X end, [5, 6])),
    Even = fun
        E(0) -> true;
        E(N) -> not E(N - 1)
    end,
    erlang:display([Even(10), Even(7)]).

errors() ->
    Down = fun D(N) when N > 0 -> D(N - 1) end,
    erlang:display(
        try Down(id(2)) of
            V -> {value, V}
        catch
            error:R -> {error, R}
        end
    ),
    Bad = fun
        B(0) -> two(B);
        B(N) -> N
    end,
    erlang:display(
        try Bad(id(0)) of
            V2 -> {value, V2}
        catch
            error:{badarity, {Fun, Args}} -> {badarity, Fun =:= Bad, Args}
        end
    ).
