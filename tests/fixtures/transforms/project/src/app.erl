-module(app).
-export([main/1]).
-compile({parse_transform, pt_rewrite}).

main(_) ->
    io:format("~p ~p~n", [double(21), shapes:area(3)]),
    io:format("~p ~p~n", [transformed(), shapes:transformed()]),
    io:format("~p~n", [dep_info()]),
    io:format("app: ok~n").
