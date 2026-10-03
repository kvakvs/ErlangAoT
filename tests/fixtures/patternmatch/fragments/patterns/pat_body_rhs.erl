-module(pat_body_rhs).
-export([f/1]).
f(V) ->
    #{K := X} = (K = V),
    X.
