-module(pat_key_bool).
-export([f/1]).
f(#{true andalso 1 := V}) -> V.
