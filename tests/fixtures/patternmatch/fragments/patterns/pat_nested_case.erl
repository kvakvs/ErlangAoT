-module(pat_nested_case).
-export([f/1]).
f({
    case a of
        a -> 1
    end
}) ->
    ok.
