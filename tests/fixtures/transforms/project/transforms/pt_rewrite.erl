-module(pt_rewrite).
-export([parse_transform/2]).

%% Rewrites calls double(X) into X * 2 and adds transformed/0, built by pt_helper.
parse_transform(Forms, _Options) ->
    Rewritten = [rewrite(Form) || Form <- Forms],
    pt_helper:add_function(Rewritten, transformed, {atom, 0, yes}).

rewrite({call, Anno, {atom, _, double}, [Argument]}) ->
    {op, Anno, '*', rewrite(Argument), {integer, Anno, 2}};
rewrite(Tuple) when is_tuple(Tuple) ->
    list_to_tuple(rewrite(tuple_to_list(Tuple)));
rewrite(List) when is_list(List) ->
    [rewrite(Element) || Element <- List];
rewrite(Other) ->
    Other.
