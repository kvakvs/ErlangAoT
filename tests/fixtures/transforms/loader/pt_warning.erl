-module(pt_warning).
-export([parse_transform/2, format_error/1]).

%% Keeps the forms and reports one warning.
parse_transform(Forms, _Options) ->
    {warning, Forms, [{"subject.erl", [{{1, 2}, ?MODULE, hello}]}]}.

format_error(hello) ->
    "pt_warning says hello".
