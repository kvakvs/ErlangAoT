-module(maybe_body).
-export([f/1]).
f(A) ->
    maybe
        {ok, X} ?= A,
        X
    end.
