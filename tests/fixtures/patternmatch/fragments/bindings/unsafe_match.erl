-module(unsafe_match).
-export([f/1]).
f(A) ->
    A andalso (X = 1),
    X = 2.
