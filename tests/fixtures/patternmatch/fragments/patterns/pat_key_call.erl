-module(pat_key_call).
-export([f/3]).
f(Position, Keys, Map) ->
    #{element(Position, Keys) := <<Count:8, Payload:Count/binary>>} = Map,
    Payload.
