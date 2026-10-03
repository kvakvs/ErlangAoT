-module(pat_prefix_tail).
-export([f/1]).
f(([1 | [2 | []]]) ++ T) -> T.
