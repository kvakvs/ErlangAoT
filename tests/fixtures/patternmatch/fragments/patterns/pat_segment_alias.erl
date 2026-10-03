-module(pat_segment_alias).
-export([f/1]).
f(<<(X = Y):8>>) -> {X, Y}.
