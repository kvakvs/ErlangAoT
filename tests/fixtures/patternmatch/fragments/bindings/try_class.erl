-module(try_class).
-export([f/1]).
f(A) ->
    try
        A
    catch
        C:R -> {C, R}
    end.
