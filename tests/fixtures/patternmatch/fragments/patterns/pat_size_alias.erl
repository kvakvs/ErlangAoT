-module(pat_size_alias).
-export([f/1]).
f(<<N:8>> = <<X:N>>) -> X.
