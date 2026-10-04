-module(case_clause_scope).
-export([f/1]).
f(A) ->
    case A of
        {X} -> X;
        _ -> X
    end.
