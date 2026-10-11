-module(cli_user).
-export([main/1]).

main(_) ->
    io:format("~p~n", [tagged()]),
    io:format("cli_user: ok~n").
