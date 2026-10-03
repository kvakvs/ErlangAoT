-module(pat_key_local_call).
-export([f/1]).
f(#{g() := V}) -> V.
g() -> a.
