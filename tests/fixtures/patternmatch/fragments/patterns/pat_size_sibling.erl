-module(pat_size_sibling).
-export([f/1]).
f({N, <<X:N>>}) -> X.
