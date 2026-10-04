-module(tool).
-export([run/1]).

%% A non-default entry function whose exit status identifies the program.
run(Args) ->
    erlang:display({tool, Args}),
    erlang:halt(4).
