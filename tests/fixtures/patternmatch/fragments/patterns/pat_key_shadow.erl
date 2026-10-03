-module(pat_key_shadow).
-export([f/1]).
f(#{length([]) := V}) -> V.
length(_) -> 0.
