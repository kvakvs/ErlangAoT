-module(answer).
-export([show/1, pair/2]).

%% Prints one term with erlang:display/1 and returns its result, the atom true.
show(Term) -> erlang:display(Term).

%% Displays both arguments in order, then the tuple built from them.
pair(First, Second) ->
    true = erlang:display(First),
    erlang:display(Second),
    erlang:display({First, Second}).
