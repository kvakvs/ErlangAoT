-module(try_of_body).
-export([f/1]).
f(A) ->
    try X = A of
        Y -> {X, Y}
    catch
        _ -> A
    end.
