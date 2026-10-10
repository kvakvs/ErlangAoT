%% A fun of an imported name is not allowed.
-module(funs).
-export([f/0]).
-import(lists, [reverse/1]).

f() -> fun reverse/1.
