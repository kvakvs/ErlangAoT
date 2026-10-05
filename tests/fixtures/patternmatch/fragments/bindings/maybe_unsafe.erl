-module(maybe_unsafe).
-export([f/1]).
f(A) ->
    maybe
        {ok, X} ?= A
    end,
    X.
