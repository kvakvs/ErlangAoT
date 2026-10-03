-module(pat_nested_block).
-export([f/1]).
f(
    {begin
        1
    end}
) ->
    ok.
