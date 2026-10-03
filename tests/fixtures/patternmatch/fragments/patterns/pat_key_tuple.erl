-module(pat_key_tuple).
-export([f/2]).
f(Offset, Map) ->
    #{{ticket, Offset + 3} := Selected} = Map,
    Selected.
