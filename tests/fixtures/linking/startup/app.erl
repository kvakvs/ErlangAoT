-module(app).
-export([main/1]).

%% Each first argument selects one exit path of the startup contract (docs/executables.md).
main(["halt"]) ->
    erlang:halt(3);
main(["halt0"]) ->
    erlang:halt();
main(["big"]) ->
    erlang:halt(1 bsl 70 + 7);
main(["slogan"]) ->
    erlang:display(before),
    erlang:halt("bye now");
main(["badhalt"]) ->
    erlang:halt(-1);
main(["crash"]) ->
    {ok, _} = helper:value();
main(["clause"]) ->
    helper:pick(none);
main(Args) ->
    erlang:display(Args),
    helper:value().
