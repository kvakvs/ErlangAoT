-module(pat_bin_noninteger_size).
-export([f/1]).
f(<<X:atom>>) -> X.
