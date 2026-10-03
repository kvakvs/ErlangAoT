-module(bit_construct_all_integer).
-export([f/1]).
f(X) -> <<X:all/integer>>.
