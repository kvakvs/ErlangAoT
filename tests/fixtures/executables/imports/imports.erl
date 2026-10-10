%% Imported functions called without their module: from a batch module, a library module and a builtin module,
%% inside funs, and one overriding an auto-imported BIF.
-module(imports).
-export([main/1]).
-compile({no_auto_import, [integer_to_list/1]}).
-import(imp_lib, [double/1, integer_to_list/1]).
-import(lists, [reverse/1, map/2, foldl/3]).
-import(io, [format/2]).

main(_) ->
    format("~p~n", [double(21)]),
    format("~p~n", [reverse([1, 2, 3])]),
    format("~p~n", [map(fun(X) -> double(X) end, [1, 2])]),
    format("~p~n", [foldl(fun(X, Sum) -> X + Sum end, 0, [1, 2, 3])]),
    format("~p~n", [integer_to_list(7)]),
    format("~p~n", [erlang:integer_to_list(7)]),
    Apply = fun reverse_twice/1,
    format("~p~n", [Apply([a, b])]).

reverse_twice(List) -> reverse(reverse(List)).
