-module(ports).
-export([main/1, id/1]).

id(X) -> X.

% is_port/1 is false for every term a program can make.
port_kind(X) when is_port(X) -> port;
port_kind(_) -> not_port.

error_of(F) ->
    try F() of
        V -> {ok, V}
    catch
        error:R -> {error, R}
    end.

main(["dynamic"]) ->
    % A port builtin reached through a dynamic call is undef: the program has no ports.
    io:format("~p~n", [
        error_of(fun() -> apply(erlang, ?MODULE:id(open_port), [{spawn, "cat"}, []]) end)
    ]);
main(_) ->
    Values = [self(), make_ref(), fun id/1, atom, 1, "#Port<0.1>", {port, 1}],
    io:format("~p~n", [[port_kind(V) || V <- Values]]),
    io:format("~p~n", [[erlang:is_port(V) || V <- Values]]),
    % There is nothing to monitor or link as a port: badarg, as for any value that is not one.
    io:format("~p~n", [
        [
            error_of(fun() -> monitor(port, self()) end),
            error_of(fun() -> link(?MODULE:id("#Port<0.1>")) end)
        ]
    ]).
