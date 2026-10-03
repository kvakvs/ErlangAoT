-module(bit_construct_unknown).
-export([f/1]).
f(X) -> <<X:8/magic>>.
