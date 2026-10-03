-module(pat_key_map).
-export([f/1]).
f(#{#{a => 1} := V}) -> V.
