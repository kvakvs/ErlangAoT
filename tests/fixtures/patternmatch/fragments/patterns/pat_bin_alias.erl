-module(pat_bin_alias).
-export([f/1]).
f(<<N:8, X:N>> = Whole) -> {Whole, X}.
