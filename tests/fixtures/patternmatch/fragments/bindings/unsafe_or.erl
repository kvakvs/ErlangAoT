-module(unsafe_or).
-export([f/1]).
f(A) ->
    A orelse (X = 1),
    X.
