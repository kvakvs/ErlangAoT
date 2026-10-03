-module(pat_key_shadow_qualified).
-export([f/1]).
f(#{erlang:length([]) := V}) -> V.
length(_) -> 0.
