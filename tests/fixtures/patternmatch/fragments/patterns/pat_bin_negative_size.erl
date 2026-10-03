-module(pat_bin_negative_size).
-export([f/1]).
f(<<X:(-1)>>) -> X.
