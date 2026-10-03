-module(rhs_first).
-export([f/0]).
f() ->
    X = Y,
    Y = 1.
