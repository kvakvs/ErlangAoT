-module(try_stack_guard).
-export([f/1]).
f(A) ->
    try
        A
    catch
        _:_:S when S =:= [] -> S
    end.
