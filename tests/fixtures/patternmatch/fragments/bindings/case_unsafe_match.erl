-module(case_unsafe_match).
-export([f/1]).
f(A) ->
    case A of
        1 -> X = a;
        _ -> ok
    end,
    X = b.
