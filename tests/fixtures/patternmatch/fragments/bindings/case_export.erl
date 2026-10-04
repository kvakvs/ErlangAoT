-module(case_export).
-export([f/1]).
f(A) ->
    case A of
        1 -> X = a;
        _ -> X = b
    end,
    X.
