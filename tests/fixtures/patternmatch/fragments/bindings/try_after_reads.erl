-module(try_after_reads).
-export([f/1]).
f(A) ->
    try
        A
    after
        {A}
    end.
