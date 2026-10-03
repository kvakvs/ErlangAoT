-module(pat_key_match).
-export([f/1]).
f(#{(X = 1) := V}) -> V.
