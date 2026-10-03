-module(pat_key_remote).
-export([f/3]).
f(X, Y, M) ->
    #{erlang:element(X, Y) := V} = M,
    V.
