%% The generated shape:behaviour_info/1 and calls of callbacks through module names.
-module(behaviours).
-export([main/1]).

main(_) ->
    io:format("~p~n", [shape:behaviour_info(callbacks)]),
    io:format("~p~n", [shape:behaviour_info(optional_callbacks)]),
    Info = fun shape:behaviour_info/1,
    io:format("~p~n", [Info(callbacks) =:= apply(shape, behaviour_info, [callbacks])]),
    Shapes = [{circle, {circle, 2}}, {square, {square, 3}}],
    io:format("~p~n", [[shape:describe(Module, Shape) || {Module, Shape} <- Shapes]]),
    io:format("~p~n", [[Module:name() || {Module, _} <- Shapes]]),
    io:format("~p~n", [
        try shape:behaviour_info(other) of
            Value -> Value
        catch
            Class:Reason -> {Class, Reason}
        end
    ]).
