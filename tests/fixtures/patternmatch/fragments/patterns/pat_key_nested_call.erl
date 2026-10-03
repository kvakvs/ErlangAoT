-module(pat_key_nested_call).
-export([f/1]).
f(#{[g()] := V}) -> V.
g() -> a.
