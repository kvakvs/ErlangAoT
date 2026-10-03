-module(pat_key_field).
-export([f/1]).
f(#{a := K, K := X}) -> X.
