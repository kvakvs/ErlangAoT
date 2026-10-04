-module(case_pattern_unsafe).
-export([f/1]).
f(A) ->
    case A of
        {X} -> ok;
        _ -> ok
    end,
    X.
