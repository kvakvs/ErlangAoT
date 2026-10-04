-module(case_sibling).
-export([f/1]).
f(A) ->
    {
        case A of
            _ -> X = 1
        end,
        X
    }.
