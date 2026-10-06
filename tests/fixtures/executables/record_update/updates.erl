-module(updates).
-export([main/1]).

-record(point, {x = 0, y = 0, z = 0}).
-record(line, {from = #point{}, to = #point{}, label}).
-record(empty, {}).
-record(counter, {name, n = 0}).

%% Displays its tag when evaluated, so the output shows the evaluation order.
trace(Tag, Value) ->
    erlang:display(Tag),
    Value.

id(Value) -> Value.

%% Single- and multi-field updates keep every other field; the field order in the source does not matter.
fields() ->
    P = #point{x = 1, y = 2, z = 3},
    erlang:display([
        P#point{x = 10},
        P#point{z = 30, x = 10},
        P#point{x = 10, y = 20, z = 30},
        P#point{},
        P#point{x = 1}
    ]),
    erlang:display(P).

%% Update values are evaluated in source order before the updated record.
order() ->
    P = #point{},
    erlang:display((trace(base, P))#point{y = trace(y, 2), x = trace(x, 1)}),
    erlang:display((trace(base, P))#point{}).

%% Nested records, chained updates and updates of records built by another module.
nested() ->
    L = #line{label = a},
    erlang:display(L#line{from = (L#line.from)#point{x = 5}, label = b}),
    erlang:display((L#line{to = #point{y = 7}})#line.to),
    erlang:display(((L#line{label = c})#line{label = d})#line{from = none}),
    erlang:display((geometry:origin())#point{z = 9}),
    erlang:display(geometry:moved((geometry:origin())#point{x = 1}, 2)),
    E = #empty{},
    erlang:display(E#empty{}).

%% A match inside an update value binds a variable that remains visible after the update.
bindings() ->
    P = #point{},
    Q0 = P#point{x = X = id(4)},
    Q = Q0#point{y = X + 1},
    erlang:display({Q, X}),
    erlang:display([P#point{x = I} || I <- [1, 2, 3]]),
    erlang:display(
        case Q of
            #point{x = 4} = R -> R#point{z = done};
            _ -> other
        end
    ).

%% Each update in a long loop allocates a new record; the old ones become garbage.
count(Counter, 0) -> Counter;
count(Counter, N) -> count(Counter#counter{n = Counter#counter.n + 1}, N - 1).

%% A value that is not this record raises {badrecord, Value} after the update values are evaluated.
caught(Value) ->
    try
        (id(Value))#point{x = trace(x, 1)}
    catch
        error:{badrecord, Bad} -> {badrecord, Bad}
    end.

failures() ->
    erlang:display([
        caught({point, 1, 2}),
        caught({point, 1, 2, 3, 4}),
        caught({line, 1, 2, 3}),
        caught({point, 1, 2, 3}),
        caught(point),
        caught([point, 1, 2, 3]),
        caught(#{x => 1})
    ]).

main(["wrong_tag"]) ->
    (id({other, 1, 2, 3}))#point{x = 1};
main(["wrong_arity"]) ->
    (id({point, 1}))#point{};
main(["not_tuple"]) ->
    (id(42))#point{y = trace(y, 2)};
main(_) ->
    fields(),
    order(),
    nested(),
    bindings(),
    erlang:display(count(#counter{name = loop}, 100000)),
    failures().
