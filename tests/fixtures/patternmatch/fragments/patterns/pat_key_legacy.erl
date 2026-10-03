-module(pat_key_legacy).
-export([f/1]).
f(#{integer(1) := V}) -> V.
