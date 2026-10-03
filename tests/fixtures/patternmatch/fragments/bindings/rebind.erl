-module(rebind).
-export([f/1]).
f(X) ->
    X = 1,
    X.
