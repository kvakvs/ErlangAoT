-module(bit_construct_conflict).
-export([f/1]).
f(X) -> <<X:8/integer-float>>.
