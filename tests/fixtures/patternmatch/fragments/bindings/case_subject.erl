-module(case_subject).
-export([f/1]).
f(A) ->
    case B = A of
        _ -> ok
    end,
    B.
