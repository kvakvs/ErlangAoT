-module(natives).
-export([main/1]).
-compile(nowarn_undefined_field).

-record(#point{x = 0, y = 0}).
-record #pair{left, right = [1, 2]}.
-record(#empty{}).
-record(#counter{n = 0}).
-record #div{class = "c"}.
-record(#flags{on = true, list = [a | b], map = #{k => {1, 2.5}}, bin = <<"ab">>, sum = 1 + 2}).
-record(tup, {a, b}).

id(X) -> X.

%% Displays its tag when evaluated, so the output shows the evaluation order.
trace(Tag, Value) ->
    erlang:display(Tag),
    Value.

%% Construction fills defaults and evaluates the given fields in source order.
construction() ->
    P = #point{y = trace(y, 2), x = trace(x, 1)},
    erlang:display({P#point.x, P#point.y}),
    Q = #point{},
    erlang:display({Q#point.x, Q#point.y}),
    erlang:display([#empty{}, #counter{}, #counter{n = #empty{}}, #div{}]),
    F = #flags{},
    erlang:display([F#flags.on, F#flags.list, F#flags.map, F#flags.bin, F#flags.sum]),
    erlang:display((#pair{left = a})#pair.right).

%% An update evaluates the record, then the new values in source order, and leaves the original unchanged.
updates() ->
    P = #point{x = 1, y = 2},
    R = (trace(base, P))#point{y = trace(y, 20), x = trace(x, 10)},
    erlang:display({R#point.x, R#point.y, P#point.x, P#point.y}),
    E = #empty{},
    erlang:display(E#empty{} =:= E),
    C = count(#counter{}, 100000),
    erlang:display(C).

count(Counter, 0) -> Counter;
count(Counter, N) -> count(Counter#counter{n = Counter#counter.n + 1}, N - 1).

%% Patterns test the record's module and name, then each listed field.
describe(#point{x = 0, y = 0}) -> origin;
describe(#point{x = X, y = X}) -> {diagonal, X};
describe(#point{x = X}) -> {point, X};
describe(#pair{left = L, right = [R | _]}) -> {pair, L, R};
describe(#empty{}) -> empty;
describe(#counter{missing = M}) -> {missing, M};
describe(#counter{n = N}) when N > 10 -> {big, N};
describe(#counter{}) -> counter;
describe(#tup{a = A}) -> {tuple_record, A};
describe(_) -> other.

patterns() ->
    erlang:display([
        describe(#point{}),
        describe(#point{x = 3, y = 3}),
        describe(#point{x = 4}),
        describe(#pair{left = l}),
        describe(#empty{}),
        describe(#counter{n = 11}),
        describe(#counter{}),
        describe(#tup{a = 1}),
        describe({point, 0, 0}),
        describe(other:point(5))
    ]),
    #pair{left = Left} = id(#pair{left = bound}),
    erlang:display(Left),
    erlang:display(
        case id(#point{x = 7}) of
            #point{x = Seven} when Seven > 5 -> {seven, Seven};
            _ -> none
        end
    ).

%% Field access in a guard fails the guard instead of raising.
classify(V) when V#point.x > 5 -> far;
classify(V) when V#point.x >= 0 -> near;
classify(_) -> unknown.

guards() ->
    erlang:display([classify(#point{x = 9}), classify(#point{}), classify(#empty{}), classify(42)]).

%% Body errors carry the offending value or the missing field.
caught(Operation) ->
    try
        run(Operation)
    catch
        error:Reason -> Reason
    end.

errors() ->
    erlang:display([
        caught(fun_access(42)),
        caught(fun_access(#tup{})),
        caught(fun_access(#empty{})),
        caught(fun_missing(#point{})),
        caught(fun_update(foo)),
        caught(fun_update(other:point(1))),
        caught(fun_unknown(#point{}))
    ]).

%% Funs are not implemented yet, so a tagged tuple selects each failing operation.
fun_access(V) -> {access, V}.
fun_missing(V) -> {missing, V}.
fun_update(V) -> {update, V}.
fun_unknown(V) -> {unknown, V}.

run({access, V}) -> (id(V))#point.x;
run({missing, V}) -> (id(V))#point.z;
run({update, V}) -> (id(V))#point{y = 1};
run({unknown, V}) -> (id(V))#point{z = 1}.

%% is_record checks native records by name and module; native records are not tuples.
tests() ->
    P = #point{},
    Foreign = other:point(1),
    erlang:display([
        is_record(P, point),
        is_record(P, empty),
        is_record(Foreign, point),
        is_record(#tup{}, tup),
        is_record(P, natives, point),
        is_record(Foreign, other, point),
        is_record(Foreign, natives, point),
        is_record(id(P), id(point)),
        is_record(id(Foreign), id(point)),
        is_record(id({point, 1}), id(point)),
        is_tuple(P),
        is_map(P)
    ]),
    %% Local field access checks only the record name, as OTP's runtime does.
    erlang:display(Foreign#point.x).

%% Records order after tuples and before maps; same records compare field by field.
order() ->
    A = #point{x = 1, y = 2},
    B = #point{x = 1, y = 3},
    erlang:display([
        A =:= #point{x = 1, y = 2},
        A =:= B,
        A < B,
        {zzz} < A,
        A < #{},
        #empty{} < A,
        A < other:point(0),
        [X#counter.n || X <- [#counter{n = 1}, #counter{n = 2}]]
    ]).

main(["badrecord"]) ->
    (id(42))#point.x;
main(["badfield"]) ->
    (id(#point{}))#point.z;
main(["update"]) ->
    (id(other:point(1)))#point{x = 2};
main(_) ->
    construction(),
    updates(),
    patterns(),
    guards(),
    errors(),
    tests(),
    order().
