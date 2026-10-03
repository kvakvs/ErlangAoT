-module(pat_key_binary).
-export([f/2]).
f(Number, Map) ->
    #{{entry, <<Number:24/little>>} := Selected} = Map,
    Selected.
