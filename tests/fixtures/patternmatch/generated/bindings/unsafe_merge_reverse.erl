-module(unsafe_merge_reverse).
-export([f/1]).
f(A) -> {X = 2, A orelse (X = 1)}, X.
