-module(pat_key_argument).
-export([f/2]).
f(K, #{K := X}) -> X.
