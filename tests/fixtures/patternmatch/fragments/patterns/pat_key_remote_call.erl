-module(pat_key_remote_call).
-export([f/1]).
f(#{other:g() := V}) -> V.
