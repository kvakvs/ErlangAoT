-module(pt_error).
-export([parse_transform/2, format_error/1]).

%% Rejects the module.
parse_transform(_Forms, _Options) ->
    {error, [{"subject.erl", [{{4, 1}, ?MODULE, nope}]}], []}.

format_error(nope) ->
    "pt_error refuses".
