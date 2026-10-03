-module(pat_segment_expr).
-export([f/1]).
f(<<N:8, (N + 1):8>>) -> N.
