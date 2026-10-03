-module(pat_prefix_variable).
-export([f/1]).
f({Xs, Xs ++ Tail}) -> Tail.
