-module(try_inside).
-export([f/1]).
f(A) ->
    try
        X = A,
        {X}
    catch
        _ -> A
    end.
