-module(pat_key_alias_reverse).
-export([f/1]).
f(#{K := X} = {K, _}) -> X.
