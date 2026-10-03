-module(pat_bin_incoming).
-export([f/2]).
f(N, B) ->
    <<X:(N + 1)>> = B,
    X.
