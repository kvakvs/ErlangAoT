-module(if_guard_unbound).
-export([f/1]).
f(A) ->
    if
        X > A -> ok;
        true -> ok
    end.
