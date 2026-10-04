-module(if_unsafe).
-export([f/1]).
f(A) ->
    if
        A > 0 -> X = a;
        true -> ok
    end,
    X.
