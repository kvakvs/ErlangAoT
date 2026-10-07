-module(anon).
-export([main/1]).

-record(#local{value = 0, extra = x}).

id(X) -> X.

%% Displays its tag when evaluated, so the output shows the evaluation order.
trace(Tag, Value) ->
    erlang:display(Tag),
    Value.

%% Anonymous access reads any native record's captured fields, even another module's private record.
reads() ->
    erlang:display([
        (id(#local{value = 1}))#_.value,
        (kinds:open(2))#_.value,
        (kinds:open(2))#_.label,
        (kinds:closed(3))#_.value
    ]).

%% An anonymous update evaluates the record first and needs it exported or defined in this module.
updates() ->
    L = (trace(base, #local{}))#_{value = trace(value, 5)},
    erlang:display({L#local.value, L#local.extra}),
    O = (kinds:open(1))#_{label = changed},
    erlang:display({O#_.value, O#_.label}),
    erlang:display((id(#local{}))#_{} =:= #local{}).

%% #_{} matches any native record; listing a field needs it exported or defined here.
shape(#_{value = V}) -> {value, V};
shape(#_{}) -> native;
shape(_) -> other.

patterns() ->
    erlang:display([
        shape(#local{value = 7}),
        shape(kinds:open(8)),
        shape(kinds:closed(9)),
        shape({local, 1, 2}),
        shape(42)
    ]).

%% A failing anonymous access fails the guard.
positive(R) when R#_.value > 0 -> yes;
positive(_) -> no.

guards() ->
    erlang:display([
        positive(#local{value = 1}), positive(kinds:closed(4)), positive(#local{}), positive(atom)
    ]).

attempt(Operation) ->
    try
        run(Operation)
    catch
        error:Reason -> Reason
    end.

run({read, V}) -> (id(V))#_.value;
run({missing, V}) -> (id(V))#_.missing;
run({update, V}) -> (id(V))#_{value = 1};
run({unknown, V}) -> (id(V))#_{missing = 1}.

errors() ->
    erlang:display([
        attempt({read, {local, 1, 2}}),
        attempt({missing, kinds:open(1)}),
        attempt({update, kinds:closed(1)}),
        attempt({update, foo}),
        attempt({unknown, #local{}})
    ]).

main(["read"]) ->
    (id(7))#_.value;
main(["update"]) ->
    (id(kinds:closed(1)))#_{value = 2};
main(_) ->
    reads(),
    updates(),
    patterns(),
    guards(),
    errors().
