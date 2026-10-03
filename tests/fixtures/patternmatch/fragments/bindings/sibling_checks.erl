-module(sibling_checks).
-export([f/0]).
f() ->
    {X = 4, _ = X = 3},
    X.
