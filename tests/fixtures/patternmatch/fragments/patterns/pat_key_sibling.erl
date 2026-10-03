-module(pat_key_sibling).
-export([f/1]).
f({K, #{K := X}}) -> X.
