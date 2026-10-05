-module(try_after_unsafe).
-export([f/1]).
f(A) ->
    try
        A
    after
        X = A
    end,
    X.
