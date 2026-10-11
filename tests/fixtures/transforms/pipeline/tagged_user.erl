-module(tagged_user).
-export([main/1]).
-compile({parse_transform, pt_tag}).

main(_) ->
    io:format("~p~n", [tagged()]),
    io:format("tagged_user: ok~n").
