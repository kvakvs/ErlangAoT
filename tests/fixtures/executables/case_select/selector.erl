-module(selector).
-export([main/1]).

-record(point, {x = 0, y = 0}).

%% Clauses are tried in order; a failing guard or a guard error falls through to the next clause.
classify(Value) ->
    case Value of
        0 -> zero;
        N when is_integer(N), N < 0 -> negative;
        N when N + 1 > 100 -> big;
        N when is_integer(N); is_float(N) -> number;
        {Tag, _} when Tag =:= ok; Tag =:= error -> result;
        {X, X} -> pair;
        #point{x = 0} -> on_axis;
        #point{} -> point;
        #{kind := Kind} -> {map, Kind};
        <<"ab", Rest/binary>> -> {prefix, Rest};
        "text" -> string;
        [Head | _] -> {head, Head};
        _ -> other
    end.

%% An already bound name in a pattern is an equality test, not a new binding.
compare(Expected, Actual) ->
    case Actual of
        Expected -> equal;
        {Expected, _} -> first;
        _ -> different
    end.

%% Nested cases, a case as the subject of a case and a case as a call argument.
nested(A, B) ->
    case A of
        x ->
            case B of
                1 ->
                    x1;
                _ ->
                    case {A, B} of
                        {x, 2} -> x2;
                        _ -> xn
                    end
            end;
        _ ->
            case
                case B of
                    1 -> one;
                    _ -> many
                end
            of
                one -> single;
                many -> plural
            end
    end.

%% The subject is evaluated once even when no clause matches; clause bodies call another module.
remote(Shape) ->
    case shapes:area(Shape) of
        {ok, Area} when Area > 10 -> {large, shapes:describe(Area)};
        {ok, Area} -> {small, shapes:describe(Area)};
        {error, Reason} -> {failed, Reason}
    end.

pick(Code) ->
    case Code of
        1 -> one;
        2 -> two
    end.

main(["clause"]) ->
    pick(3);
main(["remote"]) ->
    remote(triangle);
main(_) ->
    erlang:display([
        classify(0),
        classify(-4),
        classify(500),
        classify(atom),
        classify(7),
        classify(2.5),
        classify({ok, 1}),
        classify({error, 2}),
        classify({same, same}),
        classify(#point{y = 3}),
        classify(#point{x = 1}),
        classify(#{kind => square}),
        classify(<<"abcd">>),
        classify("text"),
        classify([z, y]),
        classify({other, 1})
    ]),
    erlang:display([compare(a, a), compare(a, {a, b}), compare(a, b)]),
    erlang:display([nested(x, 1), nested(x, 2), nested(x, 3), nested(y, 1), nested(y, 2)]),
    erlang:display([remote({square, 2}), remote({square, 4}), remote(circle)]),
    erlang:display(
        case pick(2) of
            two -> {picked, two}
        end
    ).
