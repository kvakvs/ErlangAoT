-module(try_unsafe).
-export([f/1]).
f(A) ->
    try
        X = A
    catch
        _ -> A
    end,
    X.
