-module(pat_key_operator_call).
-export([f/1]).
f(#{erlang:'+'(1, 2) := X}) -> X.
