-module(bit_construct_utf_size).
-export([f/1]).
f(X) -> <<X:1/utf8>>.
