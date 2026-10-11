-module(pt_identity).
-export([parse_transform/2]).

%% Returns the forms unchanged.
parse_transform(Forms, _Options) ->
    Forms.
