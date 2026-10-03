-module(pat_segment_tuple).
-export([f/1]).
f(<<{X}:8>>) -> X.
