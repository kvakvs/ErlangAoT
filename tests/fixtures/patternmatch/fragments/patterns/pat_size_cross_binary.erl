-module(pat_size_cross_binary).
-export([f/1]).
f({<<N:8>>, <<X:N>>}) -> X.
