-module(chain).
-export([f/0]).
f() ->
    X = Y = 1,
    {X, Y}.
