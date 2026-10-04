-module(if_export).
-export([f/1]).
f(A) ->
    if
        A > 0 -> X = a;
        true -> X = b
    end,
    X.
