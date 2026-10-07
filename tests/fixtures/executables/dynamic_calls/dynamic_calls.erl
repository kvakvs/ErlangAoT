-module(dynamic_calls).
-export([main/1, twice/1, pair/2, none/0, loop/2, apply_loop/2]).

%% One frame per step would overflow the authored runs' 4096-byte stack.
-define(STEPS, 10000).

main([]) ->
    remote_forms(),
    apply_forms(),
    fun_forms(),
    errors();
main(["loop"]) ->
    erlang:display({loop, loop(id(?STEPS), 0)});
main(["apply_loop"]) ->
    erlang:display({apply_loop, apply_loop(id(?STEPS), 0)});
main(["undef"]) ->
    M = id(dynpeer),
    M:missing(1);
main(["badarg"]) ->
    M = id(42),
    M:twice(1).

id(X) -> X.

twice(X) -> 2 * X.

pair(A, B) -> {A, B}.

none() -> none.

%% Tail calls through a runtime module name and through apply/3.
loop(0, Acc) ->
    Acc;
loop(N, Acc) ->
    M = id(dynamic_calls),
    M:loop(N - 1, Acc + 1).

apply_loop(0, Acc) -> Acc;
apply_loop(N, Acc) -> apply(id(dynamic_calls), apply_loop, [N - 1, Acc + 1]).

seq(N, N) -> [N];
seq(I, N) -> [I | seq(I + 1, N)].

remote_forms() ->
    M = id(dynpeer),
    F = id(double),
    Self = id(dynamic_calls),
    erlang:display([M:F(21), M:double(4), dynpeer:F(5), (id(dynpeer)):(id(triple))(2)]),
    erlang:display([Self:twice(3), Self:pair(a, b), Self:none()]),
    erlang:display((M:adder(5))(1)),
    erlang:display([M:F(X) || X <- [1, 2, 3]]),
    erlang:display(M:depth(id(1000))),
    erlang:display(M:visible(x)),
    erlang:display(
        (dynpeer:show(module, M)):(dynpeer:show(function, F))(dynpeer:show(argument, 7))
    ).

apply_forms() ->
    K = id(100),
    erlang:display([
        apply(fun twice/1, [4]),
        apply(fun(A, B) -> A - B end, [10, 3]),
        apply(fun(X) -> X + K end, [1]),
        apply(fun pair/2, id([x, y])),
        apply(id(fun none/0), [])
    ]),
    M = id(dynpeer),
    erlang:display([
        apply(dynpeer, double, [6]), erlang:apply(M, id(triple), [7]), apply(M, double, id([8]))
    ]),
    Fact = fun
        Fact(0) -> 1;
        Fact(N) -> N * Fact(N - 1)
    end,
    erlang:display(apply(Fact, [10])),
    erlang:display((erlang:apply(fun dynpeer:adder/1, [1]))(2)),
    erlang:display(apply(dynamic_calls, pair, seq(id(1), 2))).

fun_forms() ->
    M = id(dynpeer),
    F = id(double),
    A = id(1),
    Double = fun M:F/A,
    erlang:display([Double(5), Double =:= fun dynpeer:double/1, is_function(Double, 1)]),
    erlang:display(Double),
    Missing = fun M:missing/A,
    erlang:display([is_function(Missing), Missing == fun dynpeer:missing/1]),
    Pair = fun dynamic_calls:pair/2,
    erlang:display([
        Pair(1, 2), (apply(fun(Mod, Fn, Ar) -> fun Mod:Fn/Ar end, [dynamic_calls, none, 0]))()
    ]).

errors() ->
    Twice = fun twice/1,
    Long = seq(1, 256),
    erlang:display([
        catch_error(fun() -> (id(42)):twice(1) end),
        catch_error(fun() -> (id(dynpeer)):(id(7))(1) end),
        catch_error(fun() -> (id(nomodule)):run() end),
        catch_error(fun() -> dynpeer:(id(missing))(1) end),
        catch_error(fun() -> (id(dynpeer)):hidden(1) end),
        catch_error(fun() -> (id(dynpeer)):double(1, 2) end),
        catch_error(fun() -> apply(dynpeer, double, id([1 | 2])) end),
        catch_error(fun() -> apply(dynpeer, double, id(foo)) end),
        catch_error(fun() -> apply(id(1), id([1])) end),
        catch_error(fun() -> apply(id(1), id(foo)) end),
        catch_error(fun() -> apply(Twice, id([1 | 2])) end),
        catch_error(fun() -> apply(id({dynpeer, double}), [1]) end),
        catch_error(fun() -> apply(id(dynamic_calls), id(none), Long) end),
        catch_error(fun() -> apply(id(dynpeer), double, [1, 2]) end),
        catch_error(fun() -> apply(id(dynpeer), id("double"), [1]) end),
        catch_error(fun() -> (fun(M, F) -> fun M:F/1 end)(id(1), double) end),
        catch_error(fun() -> (fun(M, F) -> fun M:F/1 end)(dynpeer, id(2)) end),
        catch_error(fun() -> (fun(Ar) -> fun dynpeer:double/Ar end)(id(-1)) end),
        catch_error(fun() -> (fun(Ar) -> fun dynpeer:double/Ar end)(id(256)) end),
        catch_error(fun() -> (fun(Ar) -> fun dynpeer:double/Ar end)(id(one)) end)
    ]),
    erlang:display(
        try apply(Twice, id([1, 2])) of
            V -> {value, V}
        catch
            error:{badarity, {Fun, Args}} -> {badarity, Fun =:= Twice, Args}
        end
    ),
    erlang:display(
        try apply(Twice, Long) of
            V2 -> {value, V2}
        catch
            error:{badarity, {Fun2, Args2}} -> {badarity, Fun2 =:= Twice, length(Args2)}
        end
    ).

catch_error(F) ->
    try F() of
        V -> {value, V}
    catch
        error:R -> R
    end.
