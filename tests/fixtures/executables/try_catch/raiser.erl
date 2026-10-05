-module(raiser).
-export([depth/1, unmatched/1, passthrough/1]).

%% Remote helpers raise below a fixed call depth or pass an unmatched exception through their own try.
depth(N) -> {depth, one(N)}.

one(N) -> [one | two(N)].

two(0) -> [returned];
two(N) -> throw({deep, N}).

%% Only reason a is handled here; anything else leaves unchanged with its class.
unmatched(Reason) ->
    try
        error(Reason)
    catch
        error:a -> handled
    end.

passthrough(Value) ->
    try
        exit({passing, Value})
    catch
        throw:_ -> no;
        error:_ -> no
    end.
