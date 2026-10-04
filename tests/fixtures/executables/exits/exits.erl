-module(exits).
-export([main/1]).

%% The first argument selects an exit path of the executable contract; other argv is echoed.
main(["halt"]) ->
    erlang:display(stopping),
    erlang:halt(3);
main(["crash"]) ->
    {ok, _} = checks:lookup(missing);
main(["clause"]) ->
    checks:pick(none);
main(Args) ->
    erlang:display(Args).
