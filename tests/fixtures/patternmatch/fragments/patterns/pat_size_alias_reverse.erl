-module(pat_size_alias_reverse).
-export([f/1]).
f(<<X:N>> = <<N:8>>) -> X.
