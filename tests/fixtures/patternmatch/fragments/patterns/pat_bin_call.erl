-module(pat_bin_call).
-export([f/2]).
f(T, B) ->
    <<X:(tuple_size(T))>> = B,
    X.
