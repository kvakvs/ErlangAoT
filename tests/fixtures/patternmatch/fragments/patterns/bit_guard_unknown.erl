-module(bit_guard_unknown).
-export([f/1]).
f(X) when false andalso bit_size(<<X:8/magic>>) > 0 -> X;
f(_) -> no.
