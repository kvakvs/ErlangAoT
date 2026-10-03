-module(pat_key_unreachable).
-export([f/1]).
f(#{false andalso g() := V}) -> V.
g() -> a.
