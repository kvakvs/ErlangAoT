-module(pat_key_binary_string_size).
-export([f/1]).
f(#{<<"a":16>> := X}) -> X.
