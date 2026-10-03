-module(pat_size_argument).
-export([f/2]).
f(N, <<X:N>>) -> X.
