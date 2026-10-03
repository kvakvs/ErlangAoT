-module(pat_size_call).
-export([f/1]).
f(<<X:(g())>>) -> X.
g() -> 8.
