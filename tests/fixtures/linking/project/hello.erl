-module(hello).
-export([main/1]).

%% Each target compiles this module with its own LABEL definition.
main(Args) ->
    erlang:display({?LABEL, Args}).
