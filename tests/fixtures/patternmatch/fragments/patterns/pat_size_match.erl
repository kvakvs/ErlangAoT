-module(pat_size_match).
-export([f/1]).
f(<<X:(N = 8)>>) -> X.
