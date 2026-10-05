-module(catch_unsafe_match).
-export([f/1]).
f(A) ->
    _ = (catch (X = A)),
    X = A.
