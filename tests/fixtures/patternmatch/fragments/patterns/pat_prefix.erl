-module(pat_prefix).
-export([f/1]).
f("abc" ++ [65, $b] ++ Tail) -> Tail.
