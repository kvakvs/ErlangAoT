-module(catch_bound).
-export([f/1]).
f(A) ->
    X = A,
    _ = (catch (X = A)),
    X.
