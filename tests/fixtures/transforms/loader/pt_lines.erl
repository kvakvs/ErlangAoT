-module(pt_lines).
-export([parse_transform/2, parse_transform_info/0]).

%% Asks for line-only locations and returns the forms it gets.
parse_transform_info() ->
    #{error_location => line}.

parse_transform(Forms, _Options) ->
    Forms.
