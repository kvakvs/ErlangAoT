-module(pat_key_new_bif).
-export([f/2]).
f(Int, M) ->
    #{is_integer(Int, 1, 2) := V} = M,
    V.
