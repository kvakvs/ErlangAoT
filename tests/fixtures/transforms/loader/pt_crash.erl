-module(pt_crash).
-export([parse_transform/2]).

%% Raises instead of returning forms.
parse_transform(_Forms, _Options) ->
    erlang:error(boom).
