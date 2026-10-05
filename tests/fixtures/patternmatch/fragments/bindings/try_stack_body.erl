-module(try_stack_body).
-export([f/1]).
f(A) ->
    try
        A
    catch
        _:_:S -> {A, S}
    end.
