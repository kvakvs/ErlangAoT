-module(pat_unsized).
-export([f/1]).
f(<<X/binary, Y>>) -> {X, Y}.
