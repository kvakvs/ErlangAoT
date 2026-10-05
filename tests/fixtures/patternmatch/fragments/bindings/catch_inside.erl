-module(catch_inside).
-export([f/1]).
f(A) ->
    catch begin
        X = A,
        {X}
    end.
