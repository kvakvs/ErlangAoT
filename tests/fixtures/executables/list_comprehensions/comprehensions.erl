-module(comprehensions).
-export([main/1]).
-compile([nowarn_shadow_vars, nowarn_obsolete_guard, nowarn_deprecated_catch]).

%% The first argument selects a scenario; each comprehension result is displayed.
main(["basic"]) ->
    L = id([1, 2, 3, 4, 5]),
    show([X * X || X <- L]),
    show([X || X <- L, X rem 2 =:= 1]),
    show([X || X <- L, odd(X)]),
    show([{X, Y} || X <- id([1, 2, 3]), Y <- id([a, b]), X =/= 2]),
    show([{X, Y} || X <- L, X > 3, Y <- [X, X + 10]]),
    show([[Y || Y <- id([a, b]), X > 1] || X <- id([1, 2])]),
    show([X, -X || X <- id([1, 2])]),
    show([x || true]),
    show([x || id(false)]),
    show([X + 1 || X <- id("abc")]),
    show([X || X <- id([])]),
    X = id(100),
    show([X || X <- id([1, 2])]),
    show(X),
    show([Y || X <- id([[1, 2], [], [3]]), Y <- X]),
    show([X + Y || Y <- id([1, 2])]);
main(["patterns"]) ->
    show([X || {ok, X} <- id([{ok, 1}, error, {ok, 2}, {ok, 3, extra}])]),
    show([X || {X, X} <- id([{1, 1}, {1, 2}, {b, b}])]),
    show([Y || {X, Y} <- id([{1, a}, {2, b}]), X =:= 2]),
    show([X || {ok, X} <:- id([{ok, 1}, {ok, 2}])]),
    show([{X, Y} || X <- id([1, 2, 3]) && Y <- id([a, b, c])]),
    show([{X, Y, Z} || X <- id([1, 2]) && Y <- id("ab") && Z <- id([[], [x]])]),
    show([{X, Y} || {X} <- id([{1}, 2, {3}]) && Y <- id([a, b, c])]),
    show([{X, Y} || {X} <- id([{1}, 2, {3}]) && {Y} <:- id([{a}, {b}, {c}])]),
    show([X || X <- id([1, 2, 3]) && X <- id([1, 5, 3])]),
    show([{X, Y} || X <- id([1, 2]) && Y <- id([a, b]), X > 1]),
    show([{X, Y} || X <- id([1, 2]), Y <- id([c, d]) && _ <- id([e, f])]);
main(["filters"]) ->
    L = id([1, a, 3, {4}, [5]]),
    show([X || X <- L, X + 1 > 2]),
    show([X || X <- L, element(1, X) =:= 4]),
    show([X || X <- L, is_integer(X), X > 1]),
    show([X || X <- id([true, foo, false]), X]),
    show([X || X <- L, integer(X)]),
    show([X || X <- L, is_atom(X) orelse is_list(X)]),
    show(
        [
            X
         || X <- id([1, 2, 3, 4]),
            begin
                Half = X div 2,
                Half * 2 =:= X
            end
        ]
    ),
    show([trace(X) || X <- id([1, 2, 3]), trace({filter, X}) > 1]);
main(["errors"]) ->
    show(caught(fun_bad_generator)),
    show(caught(fun_bad_tail)),
    show(caught(fun_inner_generator)),
    show(caught(fun_strict)),
    show(caught(fun_zip_uneven)),
    show(caught(fun_zip_tail)),
    show(caught(fun_zip_strict)),
    show(caught(fun_bad_filter)),
    show(caught(fun_template)),
    show(caught(fun_partial)),
    show(exit_reason(catch [X || X <- id(nope)]));
main(["long"]) ->
    L = seq(1, 100000),
    Squares = [X * X || X <- L],
    show(len(Squares)),
    show(sum(Squares)),
    show(len([X || X <- L, X rem 3 =:= 0])),
    show(sum([X - Y || X <- L && Y <- L])),
    show(len([{X, Y} || X <- seq(1, 400), Y <- seq(1, 250)])),
    show(sum([double(X) || X <- L]));
main(["uncaught"]) ->
    show([X || X <- id([1, 2 | three])]).

show(Value) -> erlang:display(Value).

id(Value) -> Value.

odd(X) -> X rem 2 =:= 1.

%% The stack trace of a caught error differs from OTP's; keep the reason.
exit_reason({'EXIT', {Reason, Stack}}) when is_list(Stack) -> {'EXIT', Reason}.

double(X) -> 2 * X.

trace(Value) ->
    show(Value),
    Value.

%% Each scenario raises from inside a comprehension; the error is caught here.
caught(Name) ->
    try run(Name) of
        Value -> {value, Value}
    catch
        Class:Reason -> {Class, Reason}
    end.

run(fun_bad_generator) -> [X || X <- id(foo)];
run(fun_bad_tail) -> [trace(X) || X <- id([1, 2 | tail])];
run(fun_inner_generator) -> [{X, Y} || X <- id([1, 2]), Y <- id([X | bad])];
run(fun_strict) -> [X || {X} <:- id([{1}, 2, {3}])];
run(fun_zip_uneven) -> [{X, Y} || X <- id([1, 2]) && Y <- id([a])];
run(fun_zip_tail) -> [{X, Y} || X <- id([1 | t]) && Y <- id([a | u])];
run(fun_zip_strict) -> [{X, Y} || {X} <- id([{1}, 2, {3}]) && {Y} <:- id([{a}, b, {c}])];
run(fun_bad_filter) -> [X || X <- id([1, 2]), id(X)];
run(fun_template) -> [10 div X || X <- id([5, 2, 0, 1])];
run(fun_partial) -> [trace(X) || X <- id([1, 2, 3]), X < 3 orelse error({stop, X})].

%% A list of the integers From..To, built without deep recursion.
seq(From, To) -> seq(To, From, []).

seq(To, From, Acc) when To < From -> Acc;
seq(To, From, Acc) -> seq(To - 1, From, [To | Acc]).

len(L) -> len(L, 0).

len([], N) -> N;
len([_ | T], N) -> len(T, N + 1).

sum(L) -> sum(L, 0).

sum([], S) -> S;
sum([H | T], S) -> sum(T, S + H).
