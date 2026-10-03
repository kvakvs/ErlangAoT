-module(pat_nested_assignment_call).
-export([f/1]).
f(V) ->
    {X, length(X)} = V,
    X.
