-module(if_clause_scope).
-export([f/1]).
f(A) ->
    if
        A > 0 -> X = a;
        true -> X
    end.
