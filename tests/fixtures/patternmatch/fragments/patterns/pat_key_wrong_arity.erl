-module(pat_key_wrong_arity).
-export([f/1]).
f(#{length([], []) := V}) -> V.
