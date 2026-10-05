-module(try_clause_unsafe).
-export([f/1]).
f(A) ->
    try A of
        Y -> Y
    catch
        Y -> Y
    end,
    Y.
