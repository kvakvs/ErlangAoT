-module(pat_bin_size_container).
-export([f/1]).
f(<<X:{1, 2}>>) -> X.
