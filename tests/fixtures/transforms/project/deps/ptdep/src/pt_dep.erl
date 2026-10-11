-module(pt_dep).
-export([parse_transform/2]).

%% A transform from a dependency: adds dep_info/0, which returns from_dependency.
parse_transform(Forms, _Options) ->
    {Before, [{eof, Location} = Eof]} = lists:split(length(Forms) - 1, Forms),
    Line = erl_anno:line(Location),
    Body = [{atom, Line, from_dependency}],
    Function = {function, Line, dep_info, 0, [{clause, Line, [], [], Body}]},
    export(Before, Line) ++ [Function, Eof].

export([{attribute, _, module, _} = Module | Rest], Line) ->
    [Module, {attribute, Line, export, [{dep_info, 0}]} | Rest];
export([Form | Rest], Line) ->
    [Form | export(Rest, Line)].
