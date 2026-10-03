-module(unsafe_merge).
-export([f/1]).
f(A) ->
    {A andalso (X = 1), X = 2},
    X.
