-module(warned).
-export([main/1]).
-compile({parse_transform, pt_warning}).

main(_) ->
    io:format("warned: ok~n").
