-module(try_after_body).
-export([f/1]).
f(A) ->
    try
        X = A
    after
        X
    end.
