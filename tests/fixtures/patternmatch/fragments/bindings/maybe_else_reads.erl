-module(maybe_else_reads).
-export([f/1]).
f(A) ->
    maybe
        B = A,
        ok ?= B
    else
        E -> {A, E}
    end.
