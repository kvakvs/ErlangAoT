-module(choose).
-export([main/1]).

%% Guard sequences: ';' separates alternatives, ',' conjoins tests; the first true clause wins.
classify(X) ->
    if
        is_integer(X), X < 0 -> negative;
        X =:= 0; X == 0 -> zero;
        is_integer(X), X rem 2 =:= 0 -> even;
        is_integer(X) -> odd;
        is_float(X), X > 0 andalso X < 1 -> fraction;
        is_float(X) -> float;
        is_atom(X); is_binary(X) -> symbolic;
        true -> other
    end.

%% A guard that raises is false, so the next clause is tried.
guarded(T) ->
    if
        element(3, T) =:= c -> third;
        tuple_size(T) > 1, element(1, T) =:= a -> first;
        length(T) > 0 -> list;
        not is_tuple(T) orelse T =/= {} -> other;
        true -> fallback
    end.

%% Names bound in every clause are usable after the if.
exported(N) ->
    if
        N > 10 ->
            Size = big,
            Scale = 100;
        N > 0 ->
            Size = small,
            Scale = 10;
        true ->
            Size = none,
            Scale = 0
    end,
    {Size, Scale * N}.

%% Clause-local names stay local; only the if value leaves the expression.
local(N) ->
    Result =
        if
            N > 0 ->
                Twice = N * 2,
                Twice + 1;
            true ->
                Negated = -N,
                Negated
        end,
    Result.

%% if and case nest in both directions; an if is also an ordinary operand.
nested(A, B) ->
    case A of
        {ok, V} ->
            if
                V > B -> above;
                V < B -> below;
                true -> equal
            end;
        _ ->
            if
                is_atom(A) ->
                    case A of
                        B -> same;
                        _ -> atom
                    end;
                true ->
                    other
            end
    end.

pair(X) ->
    {
        if
            X > 0 -> positive;
            true -> nonpositive
        end,
        X
    }.

%% No true guard raises if_clause.
strict(X) ->
    if
        X =:= a -> 1;
        X =:= b -> 2
    end.

%% A body error is not retried in later clauses.
body(X) ->
    if
        X =:= 1 -> {x} = {X};
        true -> other
    end.

main(["if_clause"]) ->
    erlang:display(strict(b)),
    strict(c);
main(["body"]) ->
    body(1);
main(_) ->
    erlang:display([
        classify(-3),
        classify(0),
        classify(0.0),
        classify(4),
        classify(7),
        classify(0.5),
        classify(2.5),
        classify(ok),
        classify(<<"b">>),
        classify([1])
    ]),
    erlang:display([
        guarded({a, b, c}), guarded({a, b}), guarded({z}), guarded([1]), guarded({}), guarded(7)
    ]),
    erlang:display([exported(20), exported(3), exported(-1)]),
    erlang:display([local(4), local(-6)]),
    erlang:display([
        nested({ok, 5}, 3),
        nested({ok, 1}, 3),
        nested({ok, 3}, 3),
        nested(x, x),
        nested(x, y),
        nested(7, y)
    ]),
    erlang:display([pair(1), pair(0), strict(a), body(2)]).
