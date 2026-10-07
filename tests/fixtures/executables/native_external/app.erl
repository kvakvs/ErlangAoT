-module(app).
-export([main/1]).
-import_record(shapes, [request]).

%% A local record with the same name as a record of shapes.
-record(#circle{radius = 0}).
-record(#counter{n = 0}).
-export_record([counter]).

id(X) -> X.

%% Displays its tag when evaluated, so the output shows the evaluation order.
trace(Tag, Value) ->
    erlang:display(Tag),
    Value.

%% Qualified construction takes the defining module's defaults; imported names are qualified implicitly.
construction() ->
    C = #shapes:circle{radius = trace(radius, 5)},
    erlang:display({C#shapes:circle.radius, C#shapes:circle.center, C#shapes:circle.tags, C#shapes:circle.meta}),
    R = #request{need = trace(need, x)},
    erlang:display({R#request.need, R#request.retries}),
    Own = #app:counter{n = 1},
    erlang:display(Own#counter.n),
    erlang:display(#app:counter{}).

%% Updates evaluate the record first; reading the updated record checks module and name.
updates() ->
    C = shapes:circle(2),
    D = (trace(base, C))#shapes:circle{radius = trace(radius, 7)},
    erlang:display({D#shapes:circle.radius, C#shapes:circle.radius}),
    R = (id(#request{need = a}))#request{retries = 0},
    erlang:display(R#shapes:request.retries).

%% Patterns name the module; listing a field needs an exported record, matching the name alone does not.
describe(#shapes:circle{radius = R}) -> {circle, R};
describe(#circle{radius = R}) -> {local_circle, R};
describe(#request{need = N}) -> {request, N};
describe(#shapes:secret{code = C}) -> {secret, C};
describe(#shapes:secret{}) -> secret;
describe(_) -> other.

patterns() ->
    erlang:display([
        describe(shapes:circle(3)),
        describe(#circle{radius = 4}),
        describe(#request{need = n}),
        describe(shapes:secret()),
        describe(id({circle, 1}))
    ]).

%% Tests check module and name; the guard fails when an external access fails.
radius(V) when V#shapes:circle.radius > 2 -> big;
radius(V) when V#shapes:circle.radius >= 0 -> small;
radius(_) -> none.

tests() ->
    C = shapes:circle(1),
    erlang:display([
        is_record(C, shapes, circle),
        is_record(C, circle),
        is_record(#request{need = 1}, request),
        is_record(shapes:secret(), shapes, secret),
        is_record(C, app, circle)
    ]),
    erlang:display([radius(shapes:circle(9)), radius(C), radius(#circle{}), radius(shapes:secret())]).

%% Body errors: the record is checked before its fields.
caught({access, V}) -> attempt(fun_access, V);
caught({update, V}) -> attempt(fun_update, V);
caught({field, V}) -> attempt(fun_field, V).

attempt(Kind, V) ->
    try
        run(Kind, V)
    catch
        error:Reason -> Reason
    end.

run(fun_access, V) -> (id(V))#shapes:circle.radius;
run(fun_update, V) -> (id(V))#shapes:secret{code = 1};
run(fun_field, V) -> (id(V))#shapes:circle.missing.

errors() ->
    erlang:display([
        caught({access, #circle{}}),
        caught({access, 7}),
        caught({update, shapes:secret()}),
        caught({field, shapes:circle(1)})
    ]).

main(["private"]) ->
    #shapes:secret{};
main(["unknown_module"]) ->
    #nowhere:circle{radius = 1};
main(["unknown_field"]) ->
    #shapes:circle{depth = 1};
main(["missing"]) ->
    #request{};
main(["own_private"]) ->
    #app:circle{};
main(_) ->
    construction(),
    updates(),
    patterns(),
    tests(),
    errors().
