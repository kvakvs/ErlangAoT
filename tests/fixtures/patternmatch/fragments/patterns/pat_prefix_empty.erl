-module(pat_prefix_empty).
-export([f/1]).
f([] ++ Tail) -> Tail.
