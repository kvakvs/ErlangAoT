-module(pat_prefix_group).
-export([f/1]).
f([(1), ($a)] ++ T) -> T.
