-module(pat_size_same_segment).
-export([f/1]).
f(<<X:X>>) -> X.
