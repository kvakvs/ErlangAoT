-module(pat_prefix_atom).
-export([f/1]).
f([a] ++ Tail) -> Tail.
