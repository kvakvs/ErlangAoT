-module(try_handler_unsafe).
-export([f/1]).
f(A) ->
    try
        X = A
    catch
        _ -> X
    end.
