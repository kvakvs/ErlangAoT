-module(pat_body_chain).
-export([f/1]).
f(V) ->
    X = Y = V,
    {X, Y}.
