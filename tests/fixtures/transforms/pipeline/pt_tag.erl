-module(pt_tag).
-export([parse_transform/2]).

%% Adds and exports tagged/0, which returns {tagged, Module}.
parse_transform(Forms, _Options) ->
    {attribute, _, module, Module} = lists:keyfind(module, 3, Forms),
    {Before, [{eof, Location} = Eof]} = lists:split(length(Forms) - 1, Forms),
    Line = erl_anno:line(Location),
    Body = [{tuple, Line, [{atom, Line, tagged}, {atom, Line, Module}]}],
    Function = {function, Line, tagged, 0, [{clause, Line, [], [], Body}]},
    insert_export(Before, {attribute, Line, export, [{tagged, 0}]}) ++ [Function, Eof].

insert_export([{attribute, _, module, _} = Module | Rest], Export) ->
    [Module, Export | Rest];
insert_export([Form | Rest], Export) ->
    [Form | insert_export(Rest, Export)].
