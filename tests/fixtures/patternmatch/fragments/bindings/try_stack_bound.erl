-module(try_stack_bound).
-export([f/1]).
f(A) ->
    try
        A
    catch
        _:_:A -> A
    end.
