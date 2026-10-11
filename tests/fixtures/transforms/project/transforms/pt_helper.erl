-module(pt_helper).
-export([add_function/3]).

%% Adds an exported Name/0 that returns Value, located at the end of the module.
add_function(Forms, Name, Value) ->
    {Before, [{eof, Location} = Eof]} = lists:split(length(Forms) - 1, Forms),
    Line = erl_anno:line(Location),
    Function = {function, Line, Name, 0, [{clause, Line, [], [], [setelement(2, Value, Line)]}]},
    export(Before, Name, Line) ++ [Function, Eof].

export([{attribute, _, module, _} = Module | Rest], Name, Line) ->
    [Module, {attribute, Line, export, [{Name, 0}]} | Rest];
export([Form | Rest], Name, Line) ->
    [Form | export(Rest, Name, Line)].
