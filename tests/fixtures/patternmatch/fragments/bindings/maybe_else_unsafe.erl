-module(maybe_else_unsafe).
-export([f/1]).
f(A) ->
    maybe
        {ok, X} ?= A
    else
        _ -> X
    end.
