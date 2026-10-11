-module(pt_broken).
-export([parse_transform/2]).

%% Deliberately does not compile: the clause has no full stop.
parse_transform(Forms, _Options) ->
    Forms
