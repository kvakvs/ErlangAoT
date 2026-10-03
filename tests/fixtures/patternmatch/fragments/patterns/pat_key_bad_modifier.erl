-module(pat_key_bad_modifier).
-export([f/1]).
f(#{<<1/banana>> := X}) -> X.
