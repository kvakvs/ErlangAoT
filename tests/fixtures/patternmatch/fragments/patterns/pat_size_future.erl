-module(pat_size_future).
-export([f/1]).
f(<<X:N, N:8>>) -> X.
